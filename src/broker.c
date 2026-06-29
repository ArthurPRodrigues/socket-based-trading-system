#include "broker.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <sys/time.h>

/* ─── Min-heap de prioridade ─────────────────────────────────────────────── */

static Mensagem       *heap[MAX_FILA];
static int             heap_size = 0;
static pthread_mutex_t heap_mutex = PTHREAD_MUTEX_INITIALIZER;
static pthread_cond_t  heap_cond  = PTHREAD_COND_INITIALIZER;

/* multiplica por 10000 pra dar espaço pro TTL dentro do mesmo nível de prioridade */
static int heap_key(const Mensagem *m) {
    return (m->prioridade * 10000) + m->ttl_restante_ms;
}

static void heap_push(Mensagem *m) {
    int i = heap_size++;
    heap[i] = m;
    while (i > 0) {
        int pai = (i - 1) / 2;
        if (heap_key(heap[pai]) <= heap_key(heap[i])) break;
        Mensagem *tmp = heap[pai]; heap[pai] = heap[i]; heap[i] = tmp;
        i = pai;
    }
}

static Mensagem *heap_pop(void) {
    if (heap_size == 0) return NULL;
    Mensagem *topo = heap[0];
    heap[0] = heap[--heap_size];
    int i = 0;
    for (;;) {
        int menor = i, l = 2*i+1, r = 2*i+2;
        if (l < heap_size && heap_key(heap[l]) < heap_key(heap[menor])) menor = l;
        if (r < heap_size && heap_key(heap[r]) < heap_key(heap[menor])) menor = r;
        if (menor == i) break;
        Mensagem *tmp = heap[menor]; heap[menor] = heap[i]; heap[i] = tmp;
        i = menor;
    }
    return topo;
}

/* ─── Registro de subscribers ────────────────────────────────────────────── */

static Subscriber subscribers[MAX_SUBSCRIBERS];
static int        n_subscribers = 0;

int broker_subscribe(const char *topico, const char *host, int porta) {
    if (n_subscribers >= MAX_SUBSCRIBERS) return -1;
    for (int i = 0; i < n_subscribers; i++)
        if (strcmp(subscribers[i].topico, topico) == 0) return -1;

    strncpy(subscribers[n_subscribers].topico, topico, MAX_TOPICO - 1);
    strncpy(subscribers[n_subscribers].host,   host,   63);
    subscribers[n_subscribers].porta = porta;
    n_subscribers++;
    printf("[BROKER] Subscriber registrado: topic=%s host=%s porta=%d\n",
           topico, host, porta);
    return 0;
}

int broker_unsubscribe(const char *topico) {
    for (int i = 0; i < n_subscribers; i++) {
        if (strcmp(subscribers[i].topico, topico) == 0) {
            subscribers[i] = subscribers[--n_subscribers];
            return 0;
        }
    }
    return -1;
}

static Subscriber *encontrar_subscriber(const char *topico) {
    for (int i = 0; i < n_subscribers; i++)
        if (strcmp(subscribers[i].topico, topico) == 0)
            return &subscribers[i];
    return NULL;
}

/* ─── API de fila ────────────────────────────────────────────────────────── */

int broker_enqueue(Mensagem *msg) {
    pthread_mutex_lock(&heap_mutex);
    if (heap_size >= MAX_FILA) {
        pthread_mutex_unlock(&heap_mutex);
        return -1;
    }
    heap_push(msg);
    printf("[BROKER] Enfileirado ID:%d topic:%s prio:%d ttl:%dms (fila:%d)\n",
           msg->id_ordem, msg->topico, msg->prioridade,
           msg->ttl_restante_ms, heap_size);
    pthread_cond_signal(&heap_cond);
    pthread_mutex_unlock(&heap_mutex);
    return 0;
}

Mensagem *broker_dequeue(void) {
    pthread_mutex_lock(&heap_mutex);
    /* while porque cond_wait pode acordar sem sinal (spurious wakeup) */
    while (heap_size == 0)
        pthread_cond_wait(&heap_cond, &heap_mutex);
    Mensagem *m = heap_pop();
    pthread_mutex_unlock(&heap_mutex);
    return m;
}

/* ─── Roteamento ao subscriber ───────────────────────────────────────────── */

int broker_route(Mensagem *msg) {
    Subscriber *sub = encontrar_subscriber(msg->topico);
    if (!sub) {
        snprintf(msg->resposta, MAX_RESPOSTA, "ERR:SUBSCRIBER_INDISPONIVEL");
        return -1;
    }

    int sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock < 0) {
        snprintf(msg->resposta, MAX_RESPOSTA, "ERR:SOCKET");
        return -1;
    }

    /* evita travar indefinidamente se o subscriber não responder */
    struct timeval tv = { .tv_sec  = TIMEOUT_SUB_MS / 1000,
                          .tv_usec = (TIMEOUT_SUB_MS % 1000) * 1000 };
    setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));

    struct sockaddr_in addr = {0};
    addr.sin_family = AF_INET;
    addr.sin_port   = htons(sub->porta);

    if (inet_pton(AF_INET, sub->host, &addr.sin_addr) <= 0 ||
        connect(sock, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
        close(sock);
        snprintf(msg->resposta, MAX_RESPOSTA, "ERR:SUBSCRIBER_INDISPONIVEL");
        return -1;
    }

    send(sock, msg->payload, strlen(msg->payload), 0);

    memset(msg->resposta, 0, MAX_RESPOSTA);
    int n = read(sock, msg->resposta, MAX_RESPOSTA - 1);
    close(sock);

    if (n > 0) {
        msg->resposta[n] = '\0';
        msg->resposta[strcspn(msg->resposta, "\r\n")] = 0;
    } else {
        snprintf(msg->resposta, MAX_RESPOSTA, "ERR:TIMEOUT");
        return -1;
    }

    printf("[BROKER] Roteado ID:%d topic:%s → resposta:%s\n",
           msg->id_ordem, msg->topico, msg->resposta);
    return 0;
}

/* ─── Parsing da mensagem recebida do publisher ──────────────────────────── */

static Mensagem *parse_mensagem(const char *raw, int fd) {
    Mensagem *m = calloc(1, sizeof(Mensagem));
    if (!m) return NULL;
    m->fd_publisher = fd;

    const char *p = raw;

    if (sscanf(p, "TOPIC:%31[^;]", m->topico) != 1) goto erro;
    p = strchr(p, ';'); if (!p) goto erro; p++;

    int prio;
    if (sscanf(p, "PRIO:%d", &prio) != 1) goto erro;
    m->prioridade = (Prioridade)prio;
    p = strchr(p, ';'); if (!p) goto erro; p++;

    if (sscanf(p, "TTL:%d", &m->ttl_restante_ms) != 1) goto erro;
    p = strchr(p, ';'); if (!p) goto erro; p++;

    if (sscanf(p, "ID:%d", &m->id_ordem) != 1) goto erro;
    p = strchr(p, ';'); if (!p) goto erro; p++;

    strncpy(m->payload, p, MAX_PAYLOAD - 1);
    m->payload[strcspn(m->payload, "\r\n")] = 0;

    return m;
erro:
    free(m);
    return NULL;
}

/* ─── Thread worker (consome a fila e roteia) ────────────────────────────── */

static void *worker(void *arg) {
    (void)arg;
    for (;;) {
        Mensagem *m = broker_dequeue();

        broker_route(m);

        char saida[MAX_RESPOSTA + 8];
        snprintf(saida, sizeof(saida), "OK:%s\n", m->resposta);
        send(m->fd_publisher, saida, strlen(saida), 0);
        close(m->fd_publisher);

        free(m);
    }
    return NULL;
}

/* ─── Inicialização e loop principal ─────────────────────────────────────── */

static int server_fd = -1;

void broker_init(int porta_broker) {
    broker_subscribe("cotacao",     "127.0.0.1", 8081);
    broker_subscribe("risco",       "127.0.0.1", 8082);
    broker_subscribe("compra",      "127.0.0.1", 8083);
    broker_subscribe("compensacao", "127.0.0.1", 8083); /* mesmo serviço de compra, mas tópico separado pra priorizar rollback */

    server_fd = socket(AF_INET, SOCK_STREAM, 0);
    int opt = 1;
    setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    struct sockaddr_in addr = {0};
    addr.sin_family      = AF_INET;
    addr.sin_addr.s_addr = INADDR_ANY;
    addr.sin_port        = htons(porta_broker);

    bind(server_fd, (struct sockaddr *)&addr, sizeof(addr));
    listen(server_fd, 32);

    printf("[BROKER] Escutando na porta %d\n", porta_broker);
}

void broker_run(void) {
    pthread_t tid;
    pthread_create(&tid, NULL, worker, NULL);
    pthread_detach(tid);

    for (;;) {
        struct sockaddr_in client;
        socklen_t len = sizeof(client);
        int fd = accept(server_fd, (struct sockaddr *)&client, &len);
        if (fd < 0) continue;

        char buf[MAX_PAYLOAD + 128];
        memset(buf, 0, sizeof(buf));
        int n = read(fd, buf, sizeof(buf) - 1);
        if (n <= 0) { close(fd); continue; }
        buf[n] = '\0';

        Mensagem *m = parse_mensagem(buf, fd);
        if (!m) {
            const char *err = "ERR:FORMATO_INVALIDO\n";
            send(fd, err, strlen(err), 0);
            close(fd);
            continue;
        }

        if (broker_enqueue(m) != 0) {
            const char *err = "ERR:FILA_CHEIA\n";
            send(fd, err, strlen(err), 0);
            close(fd);
            free(m);
        }
        /* fd fica aberto aqui; o worker fecha depois de enviar a resposta */
    }
}

void broker_shutdown(void) {
    if (server_fd >= 0) {
        close(server_fd);
        server_fd = -1;
    }
}

int main(void) {
    broker_init(PORTA_BROKER);
    broker_run();
    return 0;
}
