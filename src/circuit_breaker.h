#ifndef CIRCUIT_BREAKER_H
#define CIRCUIT_BREAKER_H

typedef enum {
    CB_CLOSED,
    CB_OPEN,
    CB_HALF_OPEN
} CBState;

void cb_init(void);

int circuit_breaker_call(
    const char *topico,
    const char *payload,
    int ttl,
    int id,
    char *resposta,
    int tam_resposta
);

#endif