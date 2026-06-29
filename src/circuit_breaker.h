#ifndef CIRCUIT_BREAKER_H
#define CIRCUIT_BREAKER_H

#define MAX_FAILURES 3
#define RESET_TIMEOUT 5

/* Inicializa todos os Circuit Breakers */
void cb_init(void);

/* Chamada protegida pelo Circuit Breaker */
int circuit_breaker_call(
    const char *topico,
    const char *payload,
    int ttl_restante_ms,
    int id_ordem,
    char *resposta,
    int tam_resposta
);

/* Exibe o estado atual de todos os breakers */
void cb_print_status(void);

#endif