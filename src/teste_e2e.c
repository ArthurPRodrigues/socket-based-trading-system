#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <signal.h>
#include <sys/wait.h>

void executar_saga_trading(const char *ativo1, const char *ativo2, int ttl_max_ms);

#define N_SERVICOS 4

static pid_t pids[N_SERVICOS];

static void iniciar_servicos(int cenario) {
    /* cenario 1: compras sempre ok
     * cenario 2: compras falha na 2a chamada (ativo2 da unica saga) */
    const char *fail_on = (cenario == 2) ? "2" : "-1";

    /* broker */
    pids[0] = fork();
    if (pids[0] == 0) {
        execl("./broker", "broker", NULL);
        perror("execl broker"); exit(1);
    }

    /* cotacao */
    pids[1] = fork();
    if (pids[1] == 0) {
        execl("./sistema_cotacao", "sistema_cotacao", NULL);
        perror("execl cotacao"); exit(1);
    }

    /* risco: taxa 100% para nao interferir no cenario */
    pids[2] = fork();
    if (pids[2] == 0) {
        execl("./sistema_risco", "sistema_risco",
              "--taxa-sucesso", "100", NULL);
        perror("execl risco"); exit(1);
    }

    /* compras */
    pids[3] = fork();
    if (pids[3] == 0) {
        execl("./sistema_compras", "sistema_compras",
              "--fail-on", fail_on, NULL);
        perror("execl compras"); exit(1);
    }

    /* aguarda os servicos subirem */
    usleep(300 * 1000);
}

static void parar_servicos(void) {
    for (int i = 0; i < N_SERVICOS; i++) {
        if (pids[i] > 0) {
            kill(pids[i], SIGTERM);
            waitpid(pids[i], NULL, 0);
        }
    }
}

int main(void) {
    printf("\n==========================================\n");
    printf(" CENARIO 1: Compras efetuadas com sucesso\n");
    printf("==========================================\n");
    iniciar_servicos(1);
    executar_saga_trading("ETH/USDT", "USD/BRL", 300);
    parar_servicos();

    sleep(1);

    printf("\n==========================================\n");
    printf(" CENARIO 2: Falha na compra do Ativo 2\n");
    printf("==========================================\n");
    iniciar_servicos(2);
    executar_saga_trading("ETH/USDT", "USD/BRL", 300);
    parar_servicos();

    return 0;
}
