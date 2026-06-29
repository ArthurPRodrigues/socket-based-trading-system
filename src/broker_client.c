#include "broker_client.h"
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>

Prioridade broker_topico_para_prioridade(const char *topico) {
    if (strcmp(topico, "compensacao") == 0) return PRIO_URGENTE;
    if (strcmp(topico, "compra")      == 0) return PRIO_ALTA;
    if (strcmp(topico, "risco")       == 0) return PRIO_NORMAL;
    return PRIO_BAIXA;
}

int broker_publish(const char *broker_host, int broker_porta,
                   const char *topico, Prioridade prioridade,
                   int ttl_restante_ms, int id_ordem,
                   const char *payload,
                   char *resposta, int tam_resposta) {
    int sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock < 0) return -1;

    struct sockaddr_in addr = {0};
    addr.sin_family = AF_INET;
    addr.sin_port   = htons(broker_porta);

    if (inet_pton(AF_INET, broker_host, &addr.sin_addr) <= 0 ||
        connect(sock, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
        close(sock);
        return -1;
    }

    /* formato: TOPIC:<t>;PRIO:<p>;TTL:<ms>;ID:<id>;<payload original> */
    char msg[MAX_PAYLOAD + 128];
    snprintf(msg, sizeof(msg),
             "TOPIC:%s;PRIO:%d;TTL:%d;ID:%d;%s",
             topico, (int)prioridade, ttl_restante_ms, id_ordem, payload);

    send(sock, msg, strlen(msg), 0);

    /* broker responde "OK:<conteudo>" em sucesso ou "ERR:<motivo>" em falha */
    char buf[MAX_RESPOSTA + 8];
    memset(buf, 0, sizeof(buf));
    int n = read(sock, buf, sizeof(buf) - 1);
    close(sock);

    if (n <= 0) return -1;

    buf[n] = '\0';
    buf[strcspn(buf, "\r\n")] = 0;

    if (strncmp(buf, "OK:", 3) == 0) {
        strncpy(resposta, buf + 3, tam_resposta - 1);
        resposta[tam_resposta - 1] = '\0';
        return 0;
    }

    /* repassa o "ERR:..." pro chamador saber o motivo */
    strncpy(resposta, buf, tam_resposta - 1);
    resposta[tam_resposta - 1] = '\0';
    return -1;
}
