#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "circuit_breaker.h"
#include "saga.h"

#define TTL_CHAMADA_COTACAO_MS 60000
#define BUFFER_ENVIO 512
#define BUFFER_RESPOSTA 256

static long long obter_tempo_ms(void)
{
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (long long)ts.tv_sec * 1000LL + ts.tv_nsec / 1000000LL;
}

static int extrair_ttl(const char *resposta)
{
    const char *ttl = strstr(resposta, "TTL=");
    int valor;

    if (ttl == NULL || sscanf(ttl, "TTL=%d", &valor) != 1 || valor <= 0) return -1;
    return valor;
}

static int ttl_restante(long long inicio_validade, int ttl_ms)
{
    long long decorrido = obter_tempo_ms() - inicio_validade;
    long long restante = (long long)ttl_ms - decorrido;

    if (restante <= 0) return 0;
    if (restante > 2147483647LL) return 2147483647;
    return (int)restante;
}

static int validar_ttl(const char *momento, long long inicio_validade, int ttl_ms)
{
    int restante = ttl_restante(inicio_validade, ttl_ms);
    if (restante <= 0) {
        printf("[TTL] Expirado %s.\n", momento);
        return -1;
    }
    printf("[TTL] %s: %dms restantes.\n", momento, restante);
    return restante;
}

static int compensar_ativo(int id_ordem, const char *ativo)
{
    char requisicao[BUFFER_ENVIO];
    char resposta[BUFFER_RESPOSTA];

    snprintf(requisicao, sizeof(requisicao),
             "ID:%d;CMD:DESFAZER;ATIVO:%s", id_ordem, ativo);

    printf("[SAGA %d] Compensando %s...\n", id_ordem, ativo);
    if (circuit_breaker_call("compensacao", requisicao, TTL_CHAMADA_COTACAO_MS,
                             id_ordem, resposta, sizeof(resposta)) == 0 &&
        strcmp(resposta, "COMPENSADO") == 0) {
        printf("[SAGA %d] Compensacao de %s concluida.\n", id_ordem, ativo);
        return 0;
    }

    printf("[SAGA %d] ERRO CRITICO na compensacao de %s: %s\n",
           id_ordem, ativo, resposta);
    return -1;
}

int executar_saga_trading(const char *ativo1, const char *ativo2)
{
    static int proximo_id = 10000;
    int id_ordem = proximo_id++;
    int ttl_ms;
    int restante;
    int ativo1_comprado = 0;
    int ativo2_comprado = 0;
    long long inicio_validade;
    char requisicao[BUFFER_ENVIO];
    char resposta[BUFFER_RESPOSTA];
    char cotacao[BUFFER_RESPOSTA];

    printf("\n[SAGA %d] Ordem iniciada para %s e %s.\n", id_ordem, ativo1, ativo2);

    snprintf(requisicao, sizeof(requisicao),
             "ID:%d;CMD:COTAR;ATIVOS:%s,%s", id_ordem, ativo1, ativo2);

    if (circuit_breaker_call("cotacao", requisicao, TTL_CHAMADA_COTACAO_MS,
                             id_ordem, resposta, sizeof(resposta)) != 0) {
        printf("[SAGA %d] ABORTADA: falha na cotacao: %s\n", id_ordem, resposta);
        return -1;
    }

    ttl_ms = extrair_ttl(resposta);
    if (ttl_ms <= 0) {
        printf("[SAGA %d] ABORTADA: cotacao sem TTL valido: %s\n", id_ordem, resposta);
        return -1;
    }

    snprintf(cotacao, sizeof(cotacao), "%s", resposta);
    inicio_validade = obter_tempo_ms();
    printf("[SAGA %d][1/4] Cotacao recebida: %s\n", id_ordem, cotacao);

    restante = validar_ttl("apos a cotacao", inicio_validade, ttl_ms);
    if (restante < 0) return -1;

    restante = validar_ttl("antes do risco", inicio_validade, ttl_ms);
    if (restante < 0) return -1;

    snprintf(requisicao, sizeof(requisicao),
             "ID:%d;CMD:AVALIAR;DADOS:%s", id_ordem, cotacao);
    if (circuit_breaker_call("risco", requisicao, restante,
                             id_ordem, resposta, sizeof(resposta)) != 0) {
        printf("[SAGA %d] ABORTADA: falha na integracao de risco: %s\n",
               id_ordem, resposta);
        return -1;
    }

    restante = validar_ttl("apos o risco", inicio_validade, ttl_ms);
    if (restante < 0) return -1;

    if (strcmp(resposta, "APROVADO") != 0) {
        printf("[SAGA %d] ABORTADA: operacao rejeitada pelo risco (%s).\n",
               id_ordem, resposta);
        return -1;
    }
    printf("[SAGA %d][2/4] Risco aprovado.\n", id_ordem);

    restante = validar_ttl("antes da compra do ativo 1", inicio_validade, ttl_ms);
    if (restante < 0) return -1;

    snprintf(requisicao, sizeof(requisicao),
             "ID:%d;CMD:COMPRAR;ATIVO:%s;ORDEM_ATIVO:1", id_ordem, ativo1);
    if (circuit_breaker_call("compra", requisicao, restante,
                             id_ordem, resposta, sizeof(resposta)) != 0 ||
        strcmp(resposta, "SUCESSO") != 0) {
        printf("[SAGA %d] ABORTADA: falha na compra do ativo 1: %s\n",
               id_ordem, resposta);
        return -1;
    }
    ativo1_comprado = 1;
    printf("[SAGA %d][3/4] Ativo 1 (%s) comprado.\n", id_ordem, ativo1);

    restante = validar_ttl("apos a compra do ativo 1", inicio_validade, ttl_ms);
    if (restante < 0) goto compensacao;

    restante = validar_ttl("antes da compra do ativo 2", inicio_validade, ttl_ms);
    if (restante < 0) goto compensacao;

    snprintf(requisicao, sizeof(requisicao),
             "ID:%d;CMD:COMPRAR;ATIVO:%s;ORDEM_ATIVO:2", id_ordem, ativo2);
    if (circuit_breaker_call("compra", requisicao, restante,
                             id_ordem, resposta, sizeof(resposta)) != 0 ||
        strcmp(resposta, "SUCESSO") != 0) {
        printf("[SAGA %d] FALHA na compra do ativo 2: %s\n", id_ordem, resposta);
        goto compensacao;
    }
    ativo2_comprado = 1;
    printf("[SAGA %d][4/4] Ativo 2 (%s) comprado.\n", id_ordem, ativo2);

    restante = validar_ttl("apos a compra do ativo 2", inicio_validade, ttl_ms);
    if (restante < 0) goto compensacao;

    printf("[SAGA %d] SUCESSO: operacao concluida atomicamente.\n", id_ordem);
    return 0;

compensacao:
    printf("[SAGA %d] Iniciando transacoes compensatorias.\n", id_ordem);
    if (ativo2_comprado) compensar_ativo(id_ordem, ativo2);
    if (ativo1_comprado) compensar_ativo(id_ordem, ativo1);
    printf("[SAGA %d] Operacao cancelada.\n", id_ordem);
    return -1;
}
