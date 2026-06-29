#include <stdio.h>
#include "broker_client.h"

int main()
{
    char resposta[256];

    if(broker_publish(
            "127.0.0.1",
            8080,
            "cotacao",
            broker_topico_para_prioridade("cotacao"),
            300,
            1,
            "CMD:COTAR;ATIVOS:ETH/USDT,USD/BRL",
            resposta,
            sizeof(resposta)) == 0)
    {
        printf("Resposta: %s\n", resposta);
    }
    else
    {
        printf("Erro: %s\n", resposta);
    }

    return 0;
}