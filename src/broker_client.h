#ifndef BROKER_CLIENT_H
#define BROKER_CLIENT_H

#include "broker.h"

/*
 * Publica uma mensagem no broker e aguarda a resposta do subscriber.
 *
 * O broker encapsula a resposta como "OK:<resposta>" ou "ERR:<motivo>".
 * Em caso de sucesso, `resposta` recebe apenas o conteúdo do subscriber
 * (sem o prefixo "OK:").
 *
 * Retorna 0 em sucesso, -1 se a conexão falhar ou o broker retornar ERR.
 */
int broker_publish(const char *broker_host, int broker_porta,
                   const char *topico, Prioridade prioridade,
                   int ttl_restante_ms, int id_ordem,
                   const char *payload,
                   char *resposta, int tam_resposta);

/*
 * Converte o nome de um tópico para seu nível de prioridade padrão.
 * Útil para que o circuit breaker não precise conhecer os níveis diretamente.
 *
 *   "compensacao" → PRIO_URGENTE
 *   "compra"      → PRIO_ALTA
 *   "risco"       → PRIO_NORMAL
 *   "cotacao"     → PRIO_BAIXA
 *
 * Tópico desconhecido retorna PRIO_BAIXA.
 */
Prioridade broker_topico_para_prioridade(const char *topico);

#endif
