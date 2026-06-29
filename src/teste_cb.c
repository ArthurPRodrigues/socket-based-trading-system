#include <stdio.h>

#include "circuit_breaker.h"

int main()
{
    cb_init();

    char resposta[256];

for(int i = 0; i < 6; i++)
{
    printf("\nTentativa %d\n", i + 1);

    circuit_breaker_call(
        "cotacao",
        "CMD:COTAR;ATIVOS:ETH/USDT",
        300,
        100,
        resposta,
        sizeof(resposta)
    );

    printf("Resposta: %s\n", resposta);
    cb_print_status();
}

    return 0;
}