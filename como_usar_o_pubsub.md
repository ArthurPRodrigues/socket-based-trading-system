# Como usar o broker pub/sub

Esse arquivo é pra vocês saberem o que precisam chamar da minha parte pra o projeto funcionar junto.

---

## O que eu fiz

Criei um **broker MQ** que fica no meio entre o circuit breaker e os serviços externos (cotação, risco, compra). Em vez de vocês chamarem os serviços diretamente, vocês mandam a mensagem pro broker, ele enfileira com prioridade e repassa pro serviço certo.

Os arquivos que eu criei foram:

```
src/broker.h          → tipos e structs que todo mundo pode precisar ver
src/broker.c          → o broker em si (processo separado, tem main() próprio)
src/broker_client.h   → o que vocês vão incluir no código de vocês
src/broker_client.c   → implementação das funções que vocês vão chamar
```

Quem precisar mexer com a minha parte é principalmente **quem implementar o circuit breaker**, mas quem estiver integrando a saga também precisa saber de uma coisa (comento sobre mais abaixo).

---

## Pra quem vai implementar o circuit breaker

Você só precisa incluir o meu header de cliente:

```c
#include "broker_client.h"
```

E tem duas funções pra usar:

---

### `broker_publish` — a principal

É ela que envia a mensagem pro broker e espera a resposta.

```c
int broker_publish(
    const char *broker_host,   // endereço do broker, provavelmente "127.0.0.1"
    int         broker_porta,  // porta do broker, que é 8080 (definida em broker.h como PORTA_BROKER)
    const char *topico,        // "cotacao", "risco", "compra" ou "compensacao"
    Prioridade  prioridade,    // nível de urgência (use a função abaixo pra não ter que decorar)
    int         ttl_restante_ms, // quanto tempo ainda sobrou do TTL da cotação
    int         id_ordem,      // o ID único da transação, que vem lá do saga.c
    const char *payload,       // a mensagem em si, no mesmo formato que já existe no saga.c
    char       *resposta,      // buffer onde vai chegar a resposta do serviço
    int         tam_resposta   // tamanho do buffer
);
```

Retorna `0` se deu certo, `-1` se falhou (broker fora do ar, subscriber não respondeu, etc.).

Quando retorna `-1`, o buffer `resposta` vai ter o motivo, tipo `"ERR:TIMEOUT"` ou `"ERR:SUBSCRIBER_INDISPONIVEL"`.

**Exemplo de como ficaria no circuit breaker:**

```c
#include "broker_client.h"

// dentro da sua função do circuit breaker:
char resposta[256];
int ttl_restante = ttl_max - (obter_tempo_ms() - tempo_inicio); // vem da saga

int ok = broker_publish(
    "127.0.0.1", PORTA_BROKER,
    "cotacao",
    broker_topico_para_prioridade("cotacao"),
    ttl_restante,
    id_ordem,
    "CMD:COTAR;ATIVOS:ETH/USDT,USD/BRL",
    resposta, sizeof(resposta)
);

if (ok != 0) {
    // trata falha — o circuit breaker entra em ação aqui
}
// resposta já tem o retorno do sistema de cotação
```

---

### `broker_topico_para_prioridade` — helper pra não ter que decorar os níveis

```c
Prioridade broker_topico_para_prioridade(const char *topico);
```

Passa o nome do tópico e ela devolve a prioridade certa. Assim:

| Tópico | Prioridade |
|--------|------------|
| `"compensacao"` | PRIO_URGENTE (0) |
| `"compra"` | PRIO_ALTA (1) |
| `"risco"` | PRIO_NORMAL (2) |
| `"cotacao"` | PRIO_BAIXA (3) |

A lógica é: rollback primeiro, depois compras com TTL correndo, depois risco, e por último cotação nova.

---

## O que vem do saga.c pra vocês repassarem

O circuit breaker vai receber essas informações do saga e precisa passá-las pro `broker_publish`:

- **`id_ordem`** — já existe no saga.c como variável local. É o número aleatório gerado no início de cada transação.
- **`ttl_restante_ms`** — calculado assim:
  ```c
  int ttl_restante = ttl_max_ms - (int)(obter_tempo_ms() - tempo_inicio);
  ```
  Precisa ser recalculado **antes de cada chamada**, não uma vez só. O saga já tem `tempo_inicio` e `ttl_max_ms` definidos.

---

## Tópicos disponíveis e quem recebe

| Tópico | Porta do subscriber | Quando usar |
|--------|---------------------|-------------|
| `"cotacao"` | 8081 | Pedir cotação dos pares |
| `"risco"` | 8082 | Mandar pra análise de risco |
| `"compra"` | 8083 | Comprar ativo 1 ou ativo 2 |
| `"compensacao"` | 8083 | Desfazer compra (rollback) — vai pro mesmo serviço de compra, mas com prioridade máxima |

O payload que você passa pra cada tópico é o mesmo que já está no saga.c hoje, sem mudança.

---

## Compilar e rodar

O broker vira um executável separado. Pra compilar:

```bash
gcc -o broker src/broker.c -lpthread
```

O `broker_client.c` entra junto com os outros `.c` do sistema de operação, não precisa de executável próprio:

```bash
gcc -o sistema_operacao src/main.c src/saga.c src/gateway.c \
    src/circuit_breaker.c src/broker_client.c
```

**Ordem pra subir os processos:**

```bash
./broker &              # primeiro, sempre
./sistema_cotacao &
./sistema_risco &
./sistema_compras &
./sistema_operacao
```

---

## O que vocês NÃO precisam mexer

- `broker.h` / `broker.c` — isso é interno do broker, não precisa tocar
- A lógica de prioridade — já está encapsulada, a função `broker_topico_para_prioridade` cuida disso
- O formato da mensagem — o `broker_publish` monta o cabeçalho automaticamente, vocês só passam o payload que já existe no saga.c

Qualquer dúvida me chama.
