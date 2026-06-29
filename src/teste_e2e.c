#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <signal.h>
#include <sys/wait.h>

void executar_saga_trading(const char *ativo1, const char *ativo2, int ttl_max_ms);

#define N_SERVICOS 4

static pid_t pids[N_SERVICOS];

typedef struct {
    int cotacao_sleep_ms;
    int cotacao_taxa;
    int risco_sleep_ms;
    int risco_taxa;
    int compras_sleep_ms;
    int compras_taxa;
    int compras_fail_on;
} ConfigServicos;

static void iniciar_servicos(const ConfigServicos *cfg) {
    char s_cotacao_sleep[16], s_cotacao_taxa[16];
    char s_risco_sleep[16],   s_risco_taxa[16];
    char s_compras_sleep[16], s_compras_taxa[16], s_compras_fail[16];

    snprintf(s_cotacao_sleep, sizeof(s_cotacao_sleep), "%d", cfg->cotacao_sleep_ms);
    snprintf(s_cotacao_taxa,  sizeof(s_cotacao_taxa),  "%d", cfg->cotacao_taxa);
    snprintf(s_risco_sleep,   sizeof(s_risco_sleep),   "%d", cfg->risco_sleep_ms);
    snprintf(s_risco_taxa,    sizeof(s_risco_taxa),    "%d", cfg->risco_taxa);
    snprintf(s_compras_sleep, sizeof(s_compras_sleep), "%d", cfg->compras_sleep_ms);
    snprintf(s_compras_taxa,  sizeof(s_compras_taxa),  "%d", cfg->compras_taxa);
    snprintf(s_compras_fail,  sizeof(s_compras_fail),  "%d", cfg->compras_fail_on);

    pids[0] = fork();
    if (pids[0] == 0) {
        execl("./broker", "broker", NULL);
        perror("execl broker"); exit(1);
    }

    pids[1] = fork();
    if (pids[1] == 0) {
        execl("./sistema_cotacao", "sistema_cotacao",
              "--sleep-ms",     s_cotacao_sleep,
              "--taxa-sucesso", s_cotacao_taxa,
              NULL);
        perror("execl cotacao"); exit(1);
    }

    pids[2] = fork();
    if (pids[2] == 0) {
        execl("./sistema_risco", "sistema_risco",
              "--sleep-ms",     s_risco_sleep,
              "--taxa-sucesso", s_risco_taxa,
              NULL);
        perror("execl risco"); exit(1);
    }

    pids[3] = fork();
    if (pids[3] == 0) {
        execl("./sistema_compras", "sistema_compras",
              "--sleep-ms",     s_compras_sleep,
              "--taxa-sucesso", s_compras_taxa,
              "--fail-on",      s_compras_fail,
              NULL);
        perror("execl compras"); exit(1);
    }

    usleep(300 * 1000);
}

static void parar_servicos(void) {
    for (int i = 0; i < N_SERVICOS; i++) {
        if (pids[i] > 0) {
            kill(pids[i], SIGTERM);
            waitpid(pids[i], NULL, 0);
            pids[i] = 0;
        }
    }
}

int main(void) {
    ConfigServicos cfg;

    /* ── Cenário 1: Compras efetuadas com sucesso ── */
    printf("\n==========================================\n");
    printf(" CENARIO 1: Compras efetuadas com sucesso\n");
    printf("==========================================\n");
    cfg = (ConfigServicos){
        .cotacao_sleep_ms = 0,   .cotacao_taxa    = 100,
        .risco_sleep_ms   = 0,   .risco_taxa      = 100,
        .compras_sleep_ms = 0,   .compras_taxa    = 100,
        .compras_fail_on  = -1
    };
    iniciar_servicos(&cfg);
    executar_saga_trading("ETH/USDT", "USD/BRL", 300);
    parar_servicos();
    sleep(1);

    /* ── Cenário 2: Falha na compra do Ativo 2 ── */
    printf("\n==========================================\n");
    printf(" CENARIO 2: Falha na compra do Ativo 2\n");
    printf("==========================================\n");
    cfg = (ConfigServicos){
        .cotacao_sleep_ms = 0,   .cotacao_taxa    = 100,
        .risco_sleep_ms   = 0,   .risco_taxa      = 100,
        .compras_sleep_ms = 0,   .compras_taxa    = 100,
        .compras_fail_on  = 2
    };
    iniciar_servicos(&cfg);
    executar_saga_trading("ETH/USDT", "USD/BRL", 300);
    parar_servicos();
    sleep(1);

    /* ── Cenário 3: TTL excedido na cotação ── */
    printf("\n==========================================\n");
    printf(" CENARIO 3: Falha por TTL excedido\n");
    printf("==========================================\n");
    cfg = (ConfigServicos){
        .cotacao_sleep_ms = 400, .cotacao_taxa    = 100,
        .risco_sleep_ms   = 0,   .risco_taxa      = 100,
        .compras_sleep_ms = 0,   .compras_taxa    = 100,
        .compras_fail_on  = -1
    };
    iniciar_servicos(&cfg);
    executar_saga_trading("ETH/USDT", "USD/BRL", 300);
    parar_servicos();

    return 0;
}
