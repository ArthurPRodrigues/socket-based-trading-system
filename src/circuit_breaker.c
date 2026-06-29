#include <stdio.h>
#include <string.h>
#include <time.h>

#include "broker_client.h"
#include "circuit_breaker.h"

#define BROKER_HOST "127.0.0.1"
#define BROKER_PORT 8080

typedef enum
{
    CB_CLOSED,
    CB_OPEN,
    CB_HALF_OPEN
} CBState;

typedef struct
{
    char topico[32];
    CBState estado;
    int falhas;
    time_t instante_abertura;
} CircuitBreaker;

static CircuitBreaker breakers[] =
{
    {"cotacao",     CB_CLOSED, 0, 0},
    {"risco",       CB_CLOSED, 0, 0},
    {"compra",      CB_CLOSED, 0, 0},
    {"compensacao", CB_CLOSED, 0, 0}
};

#define NUM_BREAKERS (sizeof(breakers) / sizeof(breakers[0]))

static CircuitBreaker* obter_breaker(const char *topico);
static const char* estado_para_texto(CBState estado);
static void imprimir_breaker(const CircuitBreaker *cb);

static CircuitBreaker* obter_breaker(const char *topico)
{
    size_t i;

    if (topico == NULL)
        return NULL;

    for (i = 0; i < NUM_BREAKERS; i++)
    {
        if (strcmp(breakers[i].topico, topico) == 0)
            return &breakers[i];
    }

    return NULL;
}

static const char* estado_para_texto(CBState estado)
{
    switch (estado)
    {
        case CB_CLOSED:    return "CLOSED";
        case CB_OPEN:      return "OPEN";
        case CB_HALF_OPEN: return "HALF_OPEN";
        default:           return "UNKNOWN";
    }
}

static void imprimir_breaker(const CircuitBreaker *cb)
{
    if (cb == NULL)
        return;

    printf("Topico: %-12s | Estado: %-9s | Falhas: %d",
           cb->topico,
           estado_para_texto(cb->estado),
           cb->falhas);

    if (cb->estado == CB_OPEN)
    {
        time_t agora = time(NULL);
        int restante = RESET_TIMEOUT - (int)difftime(agora, cb->instante_abertura);

        if (restante < 0)
            restante = 0;

        printf(" | Reset em: %ds", restante);
    }

    printf("\n");
}

/* Inicializa todos os Circuit Breakers */
void cb_init(void)
{
    size_t i;

    for (i = 0; i < NUM_BREAKERS; i++)
    {
        breakers[i].estado = CB_CLOSED;
        breakers[i].falhas = 0;
        breakers[i].instante_abertura = 0;
    }
}

/* Chamada protegida pelo Circuit Breaker */
int circuit_breaker_call(
    const char *topico,
    const char *payload,
    int ttl_restante_ms,
    int id_ordem,
    char *resposta,
    int tam_resposta
)
{
    CircuitBreaker *cb;
    Prioridade prioridade;
    time_t agora;
    int rc;

    if (resposta != NULL && tam_resposta > 0)
        resposta[0] = '\0';

    if (topico == NULL || payload == NULL || resposta == NULL || tam_resposta <= 0)
        return -1;

    cb = obter_breaker(topico);
    if (cb == NULL)
    {
        snprintf(resposta, tam_resposta, "ERR:topico desconhecido");
        return -1;
    }

    if (ttl_restante_ms <= 0)
    {
        snprintf(resposta, tam_resposta, "ERR:ttl expirado");
        return -1;
    }

    agora = time(NULL);

    if (cb->estado == CB_OPEN)
    {
        double tempo_fechado = difftime(agora, cb->instante_abertura);

        if (tempo_fechado >= RESET_TIMEOUT)
        {
            cb->estado = CB_HALF_OPEN;
        }
        else
        {
            int restante = RESET_TIMEOUT - (int)tempo_fechado;
            if (restante < 0)
                restante = 0;

            snprintf(resposta, tam_resposta,
                     "ERR:circuit breaker aberto para '%s' (%ds para retry)",
                     topico, restante);
            return -1;
        }
    }

    prioridade = broker_topico_para_prioridade(topico);

    rc = broker_publish(
        BROKER_HOST,
        BROKER_PORT,
        topico,
        prioridade,
        ttl_restante_ms,
        id_ordem,
        payload,
        resposta,
        tam_resposta
    );

    if (rc == 0)
    {
        cb->falhas = 0;
        cb->estado = CB_CLOSED;
        cb->instante_abertura = 0;
        return 0;
    }

    if (resposta[0] == '\0')
    {
        snprintf(resposta, tam_resposta,
                 "ERR:falha ao comunicar com broker");
    }

    if (cb->estado == CB_HALF_OPEN)
    {
        cb->estado = CB_OPEN;
        cb->falhas = MAX_FAILURES;
        cb->instante_abertura = agora;
        return -1;
    }

    cb->falhas++;

    if (cb->falhas >= MAX_FAILURES)
    {
        cb->estado = CB_OPEN;
        cb->instante_abertura = agora;
    }
    else
    {
        cb->estado = CB_CLOSED;
    }

    return -1;
}

/* Exibe o estado atual de todos os breakers */
void cb_print_status(void)
{
    size_t i;

    printf("\n=== STATUS DOS CIRCUIT BREAKERS ===\n");
    for (i = 0; i < NUM_BREAKERS; i++)
    {
        imprimir_breaker(&breakers[i]);
    }
    printf("===================================\n");
}