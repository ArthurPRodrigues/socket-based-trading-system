#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <arpa/inet.h>
#include <sys/socket.h>

#define PORTA 8083
#define BUFFER 512

int main()
{
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
    printf(" Sistema de Compras iniciado\n");
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

        printf("\n[COMPRAS] Recebido:\n%s\n",buffer);

        if(strstr(buffer,"CMD:COMPRAR") != NULL)
        {
            printf("[COMPRAS] Compra executada.\n");

            send(client_fd,
                 "SUCESSO",
                 strlen("SUCESSO"),
                 0);
        }
        else if(strstr(buffer,"CMD:DESFAZER") != NULL)
        {
            printf("[COMPRAS] Rollback executado.\n");

            send(client_fd,
                 "COMPENSADO",
                 strlen("COMPENSADO"),
                 0);
        }
        else
        {
            printf("[COMPRAS] Comando inválido.\n");

            send(client_fd,
                 "ERRO",
                 strlen("ERRO"),
                 0);
        }

        close(client_fd);
    }

    return 0;
}