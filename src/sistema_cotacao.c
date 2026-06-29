#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <time.h>
#include <arpa/inet.h>
#include <sys/socket.h>

#define PORTA 8081
#define BUFFER 512

int main(int argc, char *argv[])
{
    int sleep_ms    = 0;
    int taxa_sucesso = 100;

    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--sleep-ms") == 0 && i + 1 < argc)
            sleep_ms = atoi(argv[++i]);
        else if (strcmp(argv[i], "--taxa-sucesso") == 0 && i + 1 < argc)
            taxa_sucesso = atoi(argv[++i]);
    }

    srand((unsigned)time(NULL));

    int server_fd, client_fd;

    struct sockaddr_in servidor;
    struct sockaddr_in cliente;

    socklen_t cliente_len = sizeof(cliente);

    server_fd = socket(AF_INET, SOCK_STREAM, 0);

    if(server_fd < 0)
    {
        perror("socket");
        return 1;
    }

    int opt = 1;
    setsockopt(server_fd,
               SOL_SOCKET,
               SO_REUSEADDR,
               &opt,
               sizeof(opt));

    servidor.sin_family = AF_INET;
    servidor.sin_addr.s_addr = INADDR_ANY;
    servidor.sin_port = htons(PORTA);

    if(bind(server_fd,
            (struct sockaddr*)&servidor,
            sizeof(servidor)) < 0)
    {
        perror("bind");
        return 1;
    }

    listen(server_fd,5);

    printf("\n=====================================\n");
    printf(" Sistema de Cotacao iniciado\n");
    printf(" Porta %d\n",PORTA);
    printf("=====================================\n");

    while(1)
    {
        client_fd = accept(server_fd,
                           (struct sockaddr*)&cliente,
                           &cliente_len);

        if(client_fd < 0)
            continue;

        char buffer[BUFFER];

        memset(buffer,0,sizeof(buffer));

        int n = read(client_fd,
                     buffer,
                     sizeof(buffer)-1);

        if(n <= 0)
        {
            close(client_fd);
            continue;
        }

        buffer[n]='\0';

        printf("\n[COTACAO] Recebido:\n%s\n",buffer);

        if (sleep_ms > 0)
            usleep(sleep_ms * 1000);

        if ((rand() % 100) >= taxa_sucesso) {
            printf("[COTACAO] Falha simulada (taxa=%d%%).\n", taxa_sucesso);
            close(client_fd);
            continue;
        }

        char resposta[BUFFER];

        sprintf(resposta,
                "ETH/USDT=2540.30;"
                "USD/BRL=5.42");

        printf("[COTACAO] Respondendo:\n%s\n",
                resposta);

        send(client_fd,
             resposta,
             strlen(resposta),
             0);

        close(client_fd);
    }

    return 0;
}