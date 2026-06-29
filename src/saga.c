#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/time.h>
#include <time.h>

#include "circuit_breaker.h"

long long obter_tempo_ms() {
    struct timeval tv;
    gettimeofday(&tv, NULL);
    return (((long long)tv.tv_sec) * 1000) + (tv.tv_usec / 1000);
}

void executar_saga_trading(const char *ativo1, const char *ativo2, int ttl_max_ms) {
    long long tempo_inicio = obter_tempo_ms();
    int ativo1_comprado = 0;
    char buffer_envio[512];
    char buffer_resposta[256];
    int ttl_restante_ms;

    cb_init();

    srand(time(NULL));
    int id_ordem = rand() % 90000 + 10000;

    printf("\n⚡ [SAGA %d] Iniciando ordem para %s e %s...\n", id_ordem, ativo1, ativo2);

    // ==========================================
    // PASSO 1: SISTEMA DE COTAÇÃO
    // ==========================================
    ttl_restante_ms = ttl_max_ms - (int)(obter_tempo_ms() - tempo_inicio);
    if (ttl_restante_ms <= 0) {
        printf("❌ [ABORTADO] TTL excedido antes da Cotação.\n");
        return;
    }

    sprintf(buffer_envio, "ID:%d;CMD:COTAR;ATIVOS:%s,%s", id_ordem, ativo1, ativo2);
    if (circuit_breaker_call("cotacao", buffer_envio, ttl_restante_ms, id_ordem,
                             buffer_resposta, sizeof(buffer_resposta)) != 0) {
        printf("❌ [ABORTADO] Falha na Cotação: %s\n", buffer_resposta);
        return;
    }
    printf("✅ [1/4] Cotação recebida: %s\n", buffer_resposta);

    // ==========================================
    // PASSO 2: SISTEMA DE RISCO
    // ==========================================
    ttl_restante_ms = ttl_max_ms - (int)(obter_tempo_ms() - tempo_inicio);
    if (ttl_restante_ms <= 0) {
        printf("❌ [ABORTADO] TTL excedido antes do Risco.\n");
        return;
    }

    sprintf(buffer_envio, "ID:%d;CMD:AVALIAR;DADOS:%s", id_ordem, buffer_resposta);
    if (circuit_breaker_call("risco", buffer_envio, ttl_restante_ms, id_ordem,
                             buffer_resposta, sizeof(buffer_resposta)) != 0 ||
        strcmp(buffer_resposta, "APROVADO") != 0) {
        printf("❌ [ABORTADO] Operação reprovada pelo Risco: %s\n", buffer_resposta);
        return;
    }
    printf("✅ [2/4] Risco Aprovado.\n");

    // ==========================================
    // PASSO 3: COMPRA DO ATIVO 1
    // ==========================================
    ttl_restante_ms = ttl_max_ms - (int)(obter_tempo_ms() - tempo_inicio);
    if (ttl_restante_ms <= 0) {
        printf("[ABORTADO] TTL excedido antes de comprar o Ativo 1.\n");
        return;
    }

    sprintf(buffer_envio, "ID:%d;CMD:COMPRAR;ATIVO:%s", id_ordem, ativo1);
    if (circuit_breaker_call("compra", buffer_envio, ttl_restante_ms, id_ordem,
                             buffer_resposta, sizeof(buffer_resposta)) == 0 &&
        strcmp(buffer_resposta, "SUCESSO") == 0) {
        ativo1_comprado = 1;
        printf("[3/4] Ativo 1 (%s) comprado com sucesso.\n", ativo1);
    } else {
        printf("[ABORTADO] Falha na compra do Ativo 1: %s\n", buffer_resposta);
        return;
    }

    // ==========================================
    // PASSO 4: COMPRA DO ATIVO 2
    // ==========================================
    ttl_restante_ms = ttl_max_ms - (int)(obter_tempo_ms() - tempo_inicio);
    if (ttl_restante_ms <= 0) {
        printf("[FALHA] TTL excedido antes do Ativo 2! Iniciando Rollback...\n");
        goto compensacao;
    }

    sprintf(buffer_envio, "ID:%d;CMD:COMPRAR;ATIVO:%s", id_ordem, ativo2);
    if (circuit_breaker_call("compra", buffer_envio, ttl_restante_ms, id_ordem,
                             buffer_resposta, sizeof(buffer_resposta)) == 0 &&
        strcmp(buffer_resposta, "SUCESSO") == 0) {
        printf("[4/4 SUCESSO] Saga concluída! %s e %s adquiridos.\n", ativo1, ativo2);
        return;
    } else {
        printf("[FALHA] Erro na compra do Ativo 2! Iniciando Rollback...\n");
    }

// ==========================================
// TRANSAÇÃO DE COMPENSAÇÃO (ROLLBACK)
// ==========================================
compensacao:
    if (ativo1_comprado) {
        /* compensação usa ttl_max_ms fresco: o rollback deve ocorrer independente do TTL da cotação */
        sprintf(buffer_envio, "ID:%d;CMD:DESFAZER;ATIVO:%s", id_ordem, ativo1);
        circuit_breaker_call("compensacao", buffer_envio, ttl_max_ms, id_ordem,
                             buffer_resposta, sizeof(buffer_resposta));
        printf("[ROLLBACK] Ativo 1 (%s) vendido/desfeito para compensar a falha.\n", ativo1);
    }
    printf("[SAGA %d] Operação inteiramente cancelada de forma atômica.\n", id_ordem);
}