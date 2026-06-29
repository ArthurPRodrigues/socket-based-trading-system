#ifndef CIRCUIT_BREAKER_H
#define CIRCUIT_BREAKER_H

/* Inicializa todos os Circuit Breakers */
void cb_init(void);

/* Realiza uma chamada protegida pelo Circuit Breaker do tópico informado */
int circuit_breaker_call(
    const char *topico,
    const char *payload,
    int ttl_restante_ms,
    int id_ordem,
    char *resposta,
    int tam_resposta
);

/* Opcional: imprime o estado de todos os breakers (debug) */
void cb_print_status(void);

#endif