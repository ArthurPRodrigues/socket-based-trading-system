#include <stdio.h>
#include <string.h>
#include <time.h>

#include "circuit_breaker.h"
#include "broker_client.h"

/* Config Circuit Breaker */

#define MAX_FAILURES 3
#define RESET_TIMEOUT 5      // segundos

#define BROKER_HOST "127.0.0.1"
#define BROKER_PORT 8080

/* Estados do Circuit Breaker */

typedef enum {
    CB_CLOSED,
    CB_OPEN,
    CB_HALF_OPEN
} CBState;

/* Estrutura do Circuit Breaker */

typedef struct {

    CBState state;

    int failures;

    time_t last_failure;

} CircuitBreaker;

static CircuitBreaker cb;

void cb_init(void)
{
    cb.state = CB_CLOSED;
    cb.failures = 0;
    cb.last_failure = 0;
}



static void imprimir_estado()
{
    switch(cb.state)
    {
        case CB_CLOSED:
            printf("[CB] Estado: CLOSED\n");
            break;

        case CB_OPEN:
            printf("[CB] Estado: OPEN\n");
            break;

        case CB_HALF_OPEN:
            printf("[CB] Estado: HALF_OPEN\n");
            break;
    }
}



int circuit_breaker_call(
    const char *topico,
    const char *payload,
    int ttl_restante_ms,
    int id_ordem,
    char *resposta,
    int tam_resposta)
{

    imprimir_estado();

    /* Se OPEN */

    if(cb.state == CB_OPEN)
    {
        time_t agora = time(NULL);

        if((agora - cb.last_failure) >= RESET_TIMEOUT)
        {
            printf("[CB] Timeout expirado. Indo para HALF_OPEN.\n");
            cb.state = CB_HALF_OPEN;
        }
        else
        {
            strcpy(resposta, "ERR:CIRCUIT_OPEN");

            printf("[CB] Requisição bloqueada.\n");

            return -1;
        }
    }

    /* Envio pro Broker */

    int status = broker_publish(
        BROKER_HOST,
        BROKER_PORT,
        topico,
        broker_topico_para_prioridade(topico),
        ttl_restante_ms,
        id_ordem,
        payload,
        resposta,
        tam_resposta
    );

    /* Em caso de Sucesso */

    if(status == 0)
    {
        if(cb.state == CB_HALF_OPEN)
        {
            printf("[CB] Serviço recuperado.\n");
        }

        cb.state = CB_CLOSED;
        cb.failures = 0;

        return 0;
    }

    /* Em caso de Falha */

    cb.failures++;

    printf("[CB] Falha %d de %d\n",
           cb.failures,
           MAX_FAILURES);

    if(cb.failures >= MAX_FAILURES)
    {
        cb.state = CB_OPEN;
        cb.last_failure = time(NULL);

        printf("[CB] Circuito ABERTO.\n");
    }
    else if(cb.state == CB_HALF_OPEN)
    {
        cb.state = CB_OPEN;
        cb.last_failure = time(NULL);

        printf("[CB] HALF_OPEN falhou. Voltando para OPEN.\n");
    }

    return -1;
}