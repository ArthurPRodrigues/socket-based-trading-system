#include <stdio.h>

// Declaramos a função que existe lá no saga.c para o main conhecê-la
void executar_saga_trading(const char *ativo1, const char *ativo2, int ttl_max_ms);

int main() {
    printf("Iniciando o Sistema de Operação (Orquestrador)...\n");

    // Executa a operação simulando os ativos pedidos no trabalho [cite: 6, 7]
    // Com um Tempo Limite de Vida (TTL) de 300 milissegundos [cite: 8]
    executar_saga_trading("ETH/USDT", "USD/BRL", 300);
    
    return 0;
}