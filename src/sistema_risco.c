#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <arpa/inet.h>
#include <sys/socket.h>

#define PORTA 8082
#define BUFFER 512

int main(int argc, char *argv[])
{
    int taxa_sucesso = 80;
    int sleep_ms = 0;

    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--taxa-sucesso") == 0 && i + 1 < argc)
            taxa_sucesso = atoi(argv[++i]);
        else if (strcmp(argv[i], "--sleep-ms") == 0 && i + 1 < argc)
            sleep_ms = atoi(argv[++i]);
    }

    srand(42);

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
    printf(" Sistema de Risco iniciado\n");
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

        printf("\n[RISCO] Requisição recebida:\n%s\n",buffer);

        if (sleep_ms > 0)
            usleep(sleep_ms * 1000);

        int aprovado = (rand() % 100) < taxa_sucesso;

        if(aprovado)
        {
            printf("[RISCO] Operação APROVADA\n");

            send(client_fd,
                 "APROVADO",
                 strlen("APROVADO"),
                 0);
        }
        else
        {
            printf("[RISCO] Operação NEGADA\n");

            send(client_fd,
                 "NEGADO",
                 strlen("NEGADO"),
                 0);
        }

        close(client_fd);
    }

    return 0;
}