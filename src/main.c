#include <stdio.h>
#include <stdlib.h>

#include "circuit_breaker.h"
#include "saga.h"

int main(int argc, char **argv)
{
    int quantidade = 1;
    int sucessos = 0;

    if (argc > 1) quantidade = atoi(argv[1]);
    if (quantidade <= 0) {
        fprintf(stderr, "Uso: %s [quantidade_de_ordens]\n", argv[0]);
        return 1;
    }

    printf("Iniciando Sistema de Operacao para %d ordem(ns).\n", quantidade);
    cb_init();

    for (int i = 0; i < quantidade; i++) {
        if (executar_saga_trading("ETH/USDT", "USD/BRL") == 0) sucessos++;
    }

    printf("\nResumo: %d sucesso(s), %d falha(s).\n",
           sucessos, quantidade - sucessos);
    cb_print_status();
    return sucessos == quantidade ? 0 : 2;
}
