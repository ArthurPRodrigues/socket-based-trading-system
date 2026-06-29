#include <stdio.h>

void executar_saga_trading(const char *ativo1, const char *ativo2, int ttl_max_ms);

int main() {
    printf("Iniciando o Sistema de Operação (Orquestrador)...\n");
    executar_saga_trading("ETH/USDT", "USD/BRL", 300);
    return 0;
}