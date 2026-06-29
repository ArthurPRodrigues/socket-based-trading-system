#include <stdio.h>
#include <string.h>
#include <time.h>

#include "broker_client.h"
#include "circuit_breaker.h"

#define BROKER_HOST "127.0.0.1"
#define BROKER_PORT 8080

typedef enum {
    CB_CLOSED,
    CB_OPEN,
    CB_HALF_OPEN
} CBState;

typedef struct {
    char topico[32];
    CBState estado;
    int falhas;
    time_t instante_abertura;
} CircuitBreaker;

static CircuitBreaker breakers[] = {
    {"cotacao",     CB_CLOSED, 0, 0},
    {"risco",       CB_CLOSED, 0, 0},
    {"compra",      CB_CLOSED, 0, 0},
    {"compensacao", CB_CLOSED, 0, 0}
};

#define NUM_BREAKERS (sizeof(breakers) / sizeof(breakers[0]))

static CircuitBreaker *obter_breaker(const char *topico)
{
    size_t i;

    if (topico == NULL) return NULL;

    for (i = 0; i < NUM_BREAKERS; i++) {
        if (strcmp(breakers[i].topico, topico) == 0) return &breakers[i];
    }
    return NULL;
}

static const char *estado_para_texto(CBState estado)
{
    switch (estado) {
        case CB_CLOSED: return "CLOSED";
        case CB_OPEN: return "OPEN";
        case CB_HALF_OPEN: return "HALF_OPEN";
        default: return "UNKNOWN";
    }
}

static void transicionar(CircuitBreaker *cb, CBState novo_estado)
{
    if (cb->estado != novo_estado) {
        printf("[CB][%s] %s -> %s\n",
               cb->topico,
               estado_para_texto(cb->estado),
               estado_para_texto(novo_estado));
        cb->estado = novo_estado;
    }
}

static void imprimir_breaker(const CircuitBreaker *cb)
{
    printf("Topico: %-12s | Estado: %-9s | Falhas: %d",
           cb->topico, estado_para_texto(cb->estado), cb->falhas);

    if (cb->estado == CB_OPEN) {
        int restante = RESET_TIMEOUT - (int)difftime(time(NULL), cb->instante_abertura);
        if (restante < 0) restante = 0;
        printf(" | Reset em: %ds", restante);
    }
    putchar('\n');
}

void cb_init(void)
{
    size_t i;

    for (i = 0; i < NUM_BREAKERS; i++) {
        breakers[i].estado = CB_CLOSED;
        breakers[i].falhas = 0;
        breakers[i].instante_abertura = 0;
    }
    printf("[CB] Circuit breakers inicializados.\n");
}

int circuit_breaker_call(
    const char *topico,
    const char *payload,
    int ttl_restante_ms,
    int id_ordem,
    char *resposta,
    int tam_resposta)
{
    CircuitBreaker *cb;
    Prioridade prioridade;
    time_t agora;
    int rc;

    if (resposta != NULL && tam_resposta > 0) resposta[0] = '\0';

    if (topico == NULL || payload == NULL || resposta == NULL || tam_resposta <= 0) {
        return -1;
    }

    cb = obter_breaker(topico);
    if (cb == NULL) {
        snprintf(resposta, (size_t)tam_resposta, "ERR:TOPICO_DESCONHECIDO");
        return -1;
    }

    if (ttl_restante_ms <= 0) {
        snprintf(resposta, (size_t)tam_resposta, "ERR:TTL_EXPIRADO");
        return -1;
    }

    agora = time(NULL);

    if (cb->estado == CB_OPEN) {
        double decorrido = difftime(agora, cb->instante_abertura);

        if (decorrido < RESET_TIMEOUT) {
            int restante = RESET_TIMEOUT - (int)decorrido;
            printf("[CB][%s] chamada bloqueada; retry em %ds\n", topico, restante);
            snprintf(resposta, (size_t)tam_resposta,
                     "ERR:CIRCUIT_BREAKER_ABERTO;RETRY:%d", restante);
            return -1;
        }
        transicionar(cb, CB_HALF_OPEN);
    }

    prioridade = broker_topico_para_prioridade(topico);
    rc = broker_publish(BROKER_HOST, BROKER_PORT, topico, prioridade,
                        ttl_restante_ms, id_ordem, payload,
                        resposta, tam_resposta);

    if (rc == 0) {
        if (cb->estado == CB_HALF_OPEN) transicionar(cb, CB_CLOSED);
        cb->falhas = 0;
        cb->instante_abertura = 0;
        return 0;
    }

    if (resposta[0] == '\0') {
        snprintf(resposta, (size_t)tam_resposta, "ERR:FALHA_COMUNICACAO_BROKER");
    }

    if (cb->estado == CB_HALF_OPEN) {
        printf("[CB][%s] chamada de teste falhou\n", topico);
        cb->falhas = MAX_FAILURES;
        cb->instante_abertura = agora;
        transicionar(cb, CB_OPEN);
        return -1;
    }

    cb->falhas++;
    printf("[CB][%s] falha %d/%d: %s\n",
           topico, cb->falhas, MAX_FAILURES, resposta);

    if (cb->falhas >= MAX_FAILURES) {
        cb->instante_abertura = agora;
        transicionar(cb, CB_OPEN);
    }

    return -1;
}

void cb_print_status(void)
{
    size_t i;

    printf("\n=== STATUS DOS CIRCUIT BREAKERS ===\n");
    for (i = 0; i < NUM_BREAKERS; i++) imprimir_breaker(&breakers[i]);
    printf("===================================\n");
}
