# Broker MQ — Interface e Especificação

Documenta todas as funções, protocolo de mensagens e pontos de integração do Broker MQ
com os demais componentes do sistema de trading.

---

## Posição na Arquitetura

```
[saga.c]
   │  chama
   ▼
[circuit_breaker.c]  ← padrão circuit breaker (implementado por outro membro)
   │  chama
   ▼
[broker_client.c]    ← biblioteca cliente (publica no broker via socket)
   │  TCP :8080
   ▼
[broker.c]           ← processo standalone (este arquivo descreve)
   │
   ├─── TCP :8081 ──► sistema_cotacao
   ├─── TCP :8082 ──► sistema_risco
   └─── TCP :8083 ──► sistema_compras
```

O broker é um **processo independente** que deve ser iniciado antes dos demais.  
Porta padrão do broker: **8080**.

---

## Protocolo de Mensagens (wire format)

Todas as mensagens trocadas via socket são strings terminadas em `\n`, com campos
separados por `;`. Isso mantém compatibilidade com o formato já adotado em `saga.c`.

### Publisher → Broker (mensagem de entrada)

```
TOPIC:<topico>;PRIO:<nivel>;TTL:<ms_restante>;ID:<id_ordem>;<payload>
```

| Campo | Tipo | Descrição |
|-------|------|-----------|
| `TOPIC` | string | Nome do tópico: `cotacao`, `risco`, `compra`, `compensacao` |
| `PRIO` | int 0–3 | Nível de prioridade (veja tabela abaixo) |
| `TTL` | int ms | Tempo restante do TTL da cotação no momento do envio |
| `ID` | int | ID único da ordem (gerado em `saga.c`) |
| payload | string | Conteúdo original da mensagem (ex: `CMD:COTAR;ATIVOS:ETH/USDT,USD/BRL`) |

Exemplo:
```
TOPIC:compra;PRIO:1;TTL:180;ID:42731;CMD:COMPRAR;ATIVO:ETH/USDT
```

### Broker → Subscriber (mensagem repassada)

O broker repassa o **payload** original ao subscriber, sem alterar o formato.  
O subscriber processa e devolve a resposta diretamente para o broker.

### Broker → Publisher (resposta)

```
OK:<resposta_do_subscriber>
```
ou em caso de erro:
```
ERR:<motivo>
```

Exemplos:
```
OK:ETH/USDT:1300.00,USD/BRL:5.00;TTL:300
OK:APROVADO
OK:SUCESSO
ERR:TTL_EXPIRADO
ERR:SUBSCRIBER_INDISPONIVEL
ERR:FILA_CHEIA
```

---

## Prioridade da Fila

O broker usa uma **fila de prioridade mínima** (min-heap) em vez de FIFO.  
Critério de ordenação (menor valor = maior urgência):

```
chave_ordenacao = (nivel_prioridade * 10000) + ttl_restante_ms
```

Dentro do mesmo nível de prioridade, mensagens com **menor TTL restante** são
atendidas primeiro — evitando que ordens prestes a expirar fiquem atrás de ordens
recém-chegadas.

| Nível | Tópico | Razão |
|-------|--------|-------|
| `0` – URGENTE | `compensacao` | Rollback financeiro deve ser imediato |
| `1` – ALTA | `compra` | TTL da cotação está contando |
| `2` – NORMAL | `risco` | Transação ativa, TTL em curso |
| `3` – BAIXA | `cotacao` | Inicia o fluxo; TTL ainda não iniciou |

---

## Funções — `broker.h` / `broker.c`

Processo standalone do broker. Compilado e executado separadamente.

```c
/* Inicializa o broker: abre socket de escuta na porta indicada e
   registra os subscribers padrão (cotacao/risco/compra/compensacao). */
void broker_init(int porta_broker);

/* Registra um subscriber para um tópico.
   Retorna 0 em sucesso, -1 se o tópico já tiver um subscriber registrado
   ou se o limite MAX_SUBSCRIBERS for atingido. */
int broker_subscribe(const char *topico, const char *host, int porta);

/* Remove o registro de um subscriber de um tópico.
   Retorna 0 em sucesso, -1 se o tópico não for encontrado. */
int broker_unsubscribe(const char *topico);

/* Insere uma mensagem na fila de prioridade.
   Retorna 0 em sucesso, -1 se a fila estiver cheia (MAX_FILA atingido). */
int broker_enqueue(Mensagem *msg);

/* Remove e retorna a mensagem de maior prioridade da fila.
   Retorna NULL se a fila estiver vazia. */
Mensagem *broker_dequeue(void);

/* Encaminha a mensagem ao subscriber do tópico correspondente via socket,
   aguarda a resposta e preenche msg->resposta.
   Retorna 0 em sucesso, -1 em falha de conexão ou timeout. */
int broker_route(Mensagem *msg);

/* Loop principal: aceita conexões de publishers, enfileira mensagens,
   despacha para subscribers e devolve respostas. Bloqueante — roda em
   thread dedicada ou é o próprio main() do processo broker. */
void broker_run(void);

/* Encerra o broker: fecha sockets abertos e libera a fila. */
void broker_shutdown(void);
```

### Estruturas internas

```c
#define MAX_FILA        128
#define MAX_SUBSCRIBERS 8
#define MAX_TOPICO      32
#define MAX_PAYLOAD     512
#define MAX_RESPOSTA    256
#define TIMEOUT_SUBSCRIBER_MS 2000

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
    int        fd_publisher;  /* socket do publisher que aguarda resposta */
} Mensagem;

typedef struct {
    char topico[MAX_TOPICO];
    char host[64];
    int  porta;
} Subscriber;
```

---

## Funções — `broker_client.h` / `broker_client.c`

Biblioteca usada pelo **circuit breaker** (e indiretamente pela saga) para publicar
mensagens no broker. Não é um processo; é incluída como arquivo `.c` no compilador.

```c
/* Publica uma mensagem no broker e aguarda a resposta.
   - broker_host / broker_porta: endereço do broker (ex: "127.0.0.1", 8080)
   - topico:   "cotacao" | "risco" | "compra" | "compensacao"
   - prioridade: valor Prioridade (0–3)
   - ttl_restante_ms: tempo restante do TTL da cotação (0 se ainda não cotou)
   - id_ordem: ID único da transação SAGA
   - payload: conteúdo da mensagem (mesmo formato de saga.c)
   - resposta / tam_resposta: buffer de saída para a resposta do subscriber
   Retorna 0 em sucesso, -1 em falha de conexão ou resposta ERR. */
int broker_publish(const char *broker_host, int broker_porta,
                   const char *topico, Prioridade prioridade,
                   int ttl_restante_ms, int id_ordem,
                   const char *payload,
                   char *resposta, int tam_resposta);

/* Converte string de tópico para o nível de prioridade padrão.
   Útil para o circuit breaker não precisar conhecer os níveis manualmente.
   Ex: broker_topico_para_prioridade("compensacao") → PRIO_URGENTE */
Prioridade broker_topico_para_prioridade(const char *topico);
```

---

## Integração com `saga.c` (via circuit breaker)

O `saga.c` atual chama `enviar_mensagem(porta, ...)` diretamente. Com o broker, a
chamada passa pelo circuit breaker, que internamente usa `broker_publish()`.

**Antes (direto):**
```c
enviar_mensagem(PORTA_COTACAO, buffer_envio, buffer_resposta, sizeof(buffer_resposta));
```

**Depois (via broker):**
```c
// circuit_breaker_call() encapsula broker_publish() + lógica de estado do CB
circuit_breaker_call("cotacao", buffer_envio, ttl_restante, id_ordem,
                     buffer_resposta, sizeof(buffer_resposta));
```

O `ttl_restante_ms` passado ao broker é sempre `ttl_max_ms - (obter_tempo_ms() - tempo_inicio)`,
calculado em `saga.c` antes de cada chamada — mantendo a lógica de TTL centralizada na saga.

---

## Tópicos e Subscribers Padrão

| Tópico | Porta | Comando esperado no payload |
|--------|-------|-----------------------------|
| `cotacao` | 8081 | `CMD:COTAR;ATIVOS:<a1>,<a2>` |
| `risco` | 8082 | `CMD:AVALIAR;DADOS:<cotacao>` |
| `compra` | 8083 | `CMD:COMPRAR;ATIVO:<ativo>` |
| `compensacao` | 8083 | `CMD:DESFAZER;ATIVO:<ativo>` |

`compensacao` e `compra` compartilham o mesmo subscriber (porta 8083), mas têm
tópicos distintos para permitir priorização independente na fila.

---

## Compilação e Execução

```bash
# Compilar o broker (processo standalone)
gcc -o broker src/broker.c -lpthread

# Compilar o sistema de operação (inclui broker_client)
gcc -o sistema_operacao src/main.c src/saga.c src/gateway.c \
    src/circuit_breaker.c src/broker_client.c

# Ordem de inicialização dos processos
./broker &              # porta 8080
./sistema_cotacao &     # porta 8081
./sistema_risco &       # porta 8082
./sistema_compras &     # porta 8083
./sistema_operacao      # dispara as ordens
```

---

## Notas de Reusabilidade

O broker é **agnóstico ao domínio**. Para usar em outro projeto:
1. Altere os subscribers registrados em `broker_init()`.
2. Adapte o mapeamento em `broker_topico_para_prioridade()` se quiser outros níveis.
3. O protocolo de mensagens (`TOPIC;PRIO;TTL;ID;payload`) permanece inalterado.
