#include "gateway.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>

int enviar_mensagem(int porta, const char *mensagem, char *resposta, int tam_resposta) {
    int sock = 0;
    struct sockaddr_in serv_addr;

    if ((sock = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
        printf("Erro na criação do socket\n");
        return -1;
    }

    serv_addr.sin_family = AF_INET;
    serv_addr.sin_port = htons(porta);

    if (inet_pton(AF_INET, "127.0.0.1", &serv_addr.sin_addr) <= 0) {
        printf("Endereço inválido\n");
        close(sock);
        return -1;
    }

    if (connect(sock, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0) {
        close(sock);
        return -1; 
    }

    send(sock, mensagem, strlen(mensagem), 0);

    memset(resposta, 0, tam_resposta);
    int valread = read(sock, resposta, tam_resposta - 1);
    
    if (valread > 0) {
        resposta[valread] = '\0';
        resposta[strcspn(resposta, "\r\n")] = 0;
    }

    close(sock);
    return 0; 
}