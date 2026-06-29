#include <arpa/inet.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <time.h>
#include <unistd.h>

#define PORTA 8082
#define BUFFER 512

static int limitar_percentual(int valor)
{
    if (valor < 0) return 0;
    if (valor > 100) return 100;
    return valor;
}

int main(int argc, char **argv)
{
    int taxa_aprovacao = argc > 1 ? limitar_percentual(atoi(argv[1])) : 80;
    int latencia_ms = argc > 2 ? atoi(argv[2]) : 50;
    int server_fd;
    struct sockaddr_in servidor = {0};

    if (latencia_ms < 0) {
        fprintf(stderr, "Uso: %s [taxa_aprovacao_0_100] [latencia_ms]\n", argv[0]);
        return 1;
    }

    srand((unsigned int)(time(NULL) ^ getpid()));
    server_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (server_fd < 0) { perror("socket"); return 1; }

    int opt = 1;
    setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    servidor.sin_family = AF_INET;
    servidor.sin_addr.s_addr = INADDR_ANY;
    servidor.sin_port = htons(PORTA);

    if (bind(server_fd, (struct sockaddr *)&servidor, sizeof(servidor)) < 0) {
        perror("bind"); close(server_fd); return 1;
    }
    if (listen(server_fd, 16) < 0) { perror("listen"); close(server_fd); return 1; }

    printf("[RISCO] porta=%d aprovacao=%d%% latencia=%dms\n",
           PORTA, taxa_aprovacao, latencia_ms);

    for (;;) {
        struct sockaddr_in cliente;
        socklen_t cliente_len = sizeof(cliente);
        int client_fd = accept(server_fd, (struct sockaddr *)&cliente, &cliente_len);
        char buffer[BUFFER] = {0};
        const char *resposta;

        if (client_fd < 0) continue;
        int n = (int)read(client_fd, buffer, sizeof(buffer) - 1);
        if (n <= 0) { close(client_fd); continue; }
        buffer[n] = '\0';

        printf("[RISCO] Recebido: %s\n", buffer);
        usleep((useconds_t)latencia_ms * 1000U);

        resposta = ((rand() % 100) < taxa_aprovacao) ? "APROVADO" : "NEGADO";
        printf("[RISCO] Resposta: %s\n", resposta);
        send(client_fd, resposta, strlen(resposta), 0);
        close(client_fd);
    }
}
