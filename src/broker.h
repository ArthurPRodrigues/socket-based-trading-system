#ifndef BROKER_H
#define BROKER_H

#define PORTA_BROKER        8080
#define MAX_FILA            128
#define MAX_SUBSCRIBERS     8
#define MAX_TOPICO          32
#define MAX_PAYLOAD         512
#define MAX_RESPOSTA        256
#define TIMEOUT_SUB_MS      2000

typedef enum {
    PRIO_URGENTE = 0,
    PRIO_ALTA    = 1,
    PRIO_NORMAL  = 2,
    PRIO_BAIXA   = 3
} Prioridade;

typedef struct {
    int        id_ordem;
    Prioridade prioridade;
    int        ttl_restante_ms;
    char       topico[MAX_TOPICO];
    char       payload[MAX_PAYLOAD];
    char       resposta[MAX_RESPOSTA];
    int        fd_publisher;
} Mensagem;

typedef struct {
    char topico[MAX_TOPICO];
    char host[64];
    int  porta;
} Subscriber;

void      broker_init(int porta_broker);
int       broker_subscribe(const char *topico, const char *host, int porta);
int       broker_unsubscribe(const char *topico);
int       broker_enqueue(Mensagem *msg);
Mensagem *broker_dequeue(void);
int       broker_route(Mensagem *msg);
void      broker_run(void);
void      broker_shutdown(void);

#endif
