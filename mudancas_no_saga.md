# Alterações em `src/saga.c`

## Motivação

O `saga.c` chamava `enviar_mensagem()` diretamente nas portas dos serviços (8081, 8082, 8083), ignorando o Circuit Breaker e o Broker MQ. A arquitetura definida exige o fluxo:

```
SAGA → Circuit Breaker → Broker MQ → Subscriber (cotação / risco / compra)
```

## O que mudou

### 1. Dependência trocada

- **Removido:** `#include "gateway.h"` e as constantes `PORTA_COTACAO`, `PORTA_RISCO`, `PORTA_COMPRAS`
- **Adicionado:** `#include "circuit_breaker.h"`

As portas dos serviços passam a ser responsabilidade do Broker, que já as conhece via `broker_subscribe()` em `broker_init()`.

### 2. Chamadas substituídas

Cada `enviar_mensagem(PORTA_X, payload, resposta, tam)` foi substituída por:

```c
circuit_breaker_call("topico", payload, ttl_restante_ms, id_ordem, resposta, tam)
```

| Passo | Antes | Depois |
|---|---|---|
| Cotação | `enviar_mensagem(8081, ...)` | `circuit_breaker_call("cotacao", ...)` |
| Risco | `enviar_mensagem(8082, ...)` | `circuit_breaker_call("risco", ...)` |
| Compra ativo 1 | `enviar_mensagem(8083, ...)` | `circuit_breaker_call("compra", ...)` |
| Compra ativo 2 | `enviar_mensagem(8083, ...)` | `circuit_breaker_call("compra", ...)` |
| Compensação | `enviar_mensagem(8083, ...)` | `circuit_breaker_call("compensacao", ...)` |

O tópico `"compensacao"` é mapeado para `PRIO_URGENTE` no broker, garantindo que o rollback seja processado antes de qualquer compra pendente na fila.

### 3. TTL restante calculado em cada passo

O `circuit_breaker_call()` requer o TTL restante (não o TTL total). Antes de cada chamada agora é calculado:

```c
ttl_restante_ms = ttl_max_ms - (int)(obter_tempo_ms() - tempo_inicio);
```

Isso permite que o Circuit Breaker e o Broker recebam o tempo real disponível para aquela etapa.

### 4. Compensação usa TTL fresco

A transação de compensação (rollback) recebe `ttl_max_ms` em vez do TTL restante, porque o rollback deve ocorrer mesmo que o TTL da cotação já tenha expirado — são operações de natureza diferente.

### 5. `cb_init()` adicionado no início da função

Garante que os Circuit Breakers começam no estado `CLOSED` a cada execução da saga.

## Arquivos impactados

| Arquivo | Alteração |
|---|---|
| `src/saga.c` | Substituição das chamadas diretas pelo Circuit Breaker |
| `src/gateway.c` / `src/gateway.h` | Não alterados, mas não são mais usados pelo `saga.c` |
| `src/circuit_breaker.c` / `.h` | Não alterados |
| `src/broker.c` / `broker_client.c` | Não alterados |
