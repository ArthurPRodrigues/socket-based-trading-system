#include <stdio.h>
#include <string.h>
#include <time.h>

#include "circuit_breaker.h"
#include "broker_client.h"

#define MAX_FAILURES 3
#define RESET_TIMEOUT 5

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

    CBState state;

    int failures;

    time_t last_failure;

} CircuitBreaker;


/*==========================================================
    Um breaker para cada tópico
==========================================================*/

static CircuitBreaker breakers[] =
{
    {"cotacao",      CB_CLOSED,0,0},
    {"risco",        CB_CLOSED,0,0},
    {"compra",       CB_CLOSED,0,0},
    {"compensacao",  CB_CLOSED,0,0}
};

#define NUM_BREAKERS (sizeof(breakers)/sizeof(CircuitBreaker))

/*==========================================================
    Procura o breaker do tópico
==========================================================*/

static CircuitBreaker* obter_breaker(const char *topico)
{
    int i;

    for(i=0;i<NUM_BREAKERS;i++)
    {
        if(strcmp(breakers[i].topico,topico)==0)
            return &breakers[i];
    }

    return NULL;
}

/*==========================================================
    Inicialização
==========================================================*/

void cb_init(void)
{
    int i;

    for(i=0;i<NUM_BREAKERS;i++)
    {
        breakers[i].state = CB_CLOSED;
        breakers[i].failures = 0;
        breakers[i].last_failure = 0;
    }
}

/*==========================================================
    Apenas para debug
==========================================================*/

static const char* nome_estado(CBState estado)
{
    switch(estado)
    {
        case CB_CLOSED:
            return "CLOSED";

        case CB_OPEN:
            return "OPEN";

        case CB_HALF_OPEN:
            return "HALF_OPEN";
    }

    return "DESCONHECIDO";
}

/*==========================================================
    Circuit Breaker
==========================================================*/

int circuit_breaker_call(
        const char *topico,
        const char *payload,
        int ttl_restante_ms,
        int id_ordem,
        char *resposta,
        int tam_resposta)
{

    CircuitBreaker *cb = obter_breaker(topico);

    if(cb == NULL)
    {
        strcpy(resposta,"ERR:TOPICO_INVALIDO");
        return -1;
    }

    printf("\n=============================\n");
    printf("TOPICO: %s\n", cb->topico);
    printf("ESTADO: %s\n", nome_estado(cb->state));
    printf("=============================\n");

    /*--------------------------------------------
        Estado OPEN
    --------------------------------------------*/

    if(cb->state == CB_OPEN)
    {
        time_t agora = time(NULL);

        if((agora - cb->last_failure) >= RESET_TIMEOUT)
        {
            printf("[CB] Timeout expirou.\n");
            printf("[CB] OPEN -> HALF_OPEN\n");

            cb->state = CB_HALF_OPEN;
        }
        else
        {
            printf("[CB] Requisição bloqueada.\n");

            strcpy(resposta,"ERR:CIRCUIT_OPEN");

            return -1;
        }
    }

    /*--------------------------------------------
        Envia ao Broker
    --------------------------------------------*/

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

    /*--------------------------------------------
        SUCESSO
    --------------------------------------------*/

    if(status == 0)
    {
        printf("[CB] Sucesso.\n");

        if(cb->state == CB_HALF_OPEN)
        {
            printf("[CB] HALF_OPEN -> CLOSED\n");
        }

        cb->state = CB_CLOSED;
        cb->failures = 0;

        return 0;
    }

    /*--------------------------------------------
        FALHA
    --------------------------------------------*/

    cb->failures++;

    printf("[CB] Falha %d/%d\n",
           cb->failures,
           MAX_FAILURES);

    if(cb->state == CB_HALF_OPEN)
    {
        printf("[CB] HALF_OPEN -> OPEN\n");

        cb->state = CB_OPEN;
        cb->last_failure = time(NULL);

        return -1;
    }

    if(cb->failures >= MAX_FAILURES)
    {
        printf("[CB] CLOSED -> OPEN\n");

        cb->state = CB_OPEN;
        cb->last_failure = time(NULL);
    }

    return -1;
}

void cb_print_status(void)
{
    int i;

    printf("\n=========== CIRCUIT BREAKERS ===========\n");

    for(i = 0; i < NUM_BREAKERS; i++)
    {
        printf("%-15s | %-10s | Falhas: %d\n",
               breakers[i].topico,
               nome_estado(breakers[i].state),
               breakers[i].failures);
    }

    printf("========================================\n");
}