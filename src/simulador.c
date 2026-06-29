#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <signal.h>
#include <sys/wait.h>

void executar_saga_trading(const char *ativo1, const char *ativo2, int ttl_max_ms);

#define N_SERVICOS 4

static pid_t pids[N_SERVICOS];

static int ler_int(const char *prompt, int def) {
    char buf[64];
    printf("  %s [%d]: ", prompt, def);
    fflush(stdout);
    if (fgets(buf, sizeof(buf), stdin) == NULL || buf[0] == '\n')
        return def;
    return atoi(buf);
}

static void ler_str(const char *prompt, const char *def, char *out, int size) {
    printf("  %s [%s]: ", prompt, def);
    fflush(stdout);
    if (fgets(out, size, stdin) == NULL || out[0] == '\n') {
        strncpy(out, def, size - 1);
        out[size - 1] = '\0';
        return;
    }
    int len = strlen(out);
    if (len > 0 && out[len - 1] == '\n')
        out[len - 1] = '\0';
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
    char buf[8];

    printf("\n==========================================\n");
    printf("   SIMULADOR DE CENARIOS DE TRADING\n");
    printf("==========================================\n");

    for (;;) {
        printf("\n--- Configure os servicos ---\n");

        printf("\n[COTACAO]\n");
        int cotacao_sleep = ler_int("Sleep (ms)", 0);
        int cotacao_taxa  = ler_int("Taxa de sucesso (%)", 100);

        printf("\n[RISCO]\n");
        int risco_sleep = ler_int("Sleep (ms)", 0);
        int risco_taxa  = ler_int("Taxa de sucesso (%)", 80);

        printf("\n[COMPRAS]\n");
        int compras_sleep   = ler_int("Sleep (ms)", 0);
        int compras_taxa    = ler_int("Taxa de sucesso (%)", 100);
        int compras_fail_on = ler_int("Forcar falha na chamada N (-1 = nunca)", -1);

        printf("\n[SAGA]\n");
        char ativo1[64], ativo2[64];
        ler_str("Ativo 1", "ETH/USDT", ativo1, sizeof(ativo1));
        ler_str("Ativo 2", "USD/BRL",  ativo2, sizeof(ativo2));
        int ttl = ler_int("TTL (ms)", 300);

        char s_cotacao_sleep[16], s_cotacao_taxa[16];
        char s_risco_sleep[16],   s_risco_taxa[16];
        char s_compras_sleep[16], s_compras_taxa[16], s_compras_fail[16];

        snprintf(s_cotacao_sleep, sizeof(s_cotacao_sleep), "%d", cotacao_sleep);
        snprintf(s_cotacao_taxa,  sizeof(s_cotacao_taxa),  "%d", cotacao_taxa);
        snprintf(s_risco_sleep,   sizeof(s_risco_sleep),   "%d", risco_sleep);
        snprintf(s_risco_taxa,    sizeof(s_risco_taxa),    "%d", risco_taxa);
        snprintf(s_compras_sleep, sizeof(s_compras_sleep), "%d", compras_sleep);
        snprintf(s_compras_taxa,  sizeof(s_compras_taxa),  "%d", compras_taxa);
        snprintf(s_compras_fail,  sizeof(s_compras_fail),  "%d", compras_fail_on);

        printf("\n--- Iniciando servicos ---\n");

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

        printf("\n--- Executando saga ---\n");
        executar_saga_trading(ativo1, ativo2, ttl);

        parar_servicos();
        sleep(1);

        printf("\nRodar outro cenario? (s/n): ");
        fflush(stdout);
        if (fgets(buf, sizeof(buf), stdin) == NULL)
            break;
        if (buf[0] != 's' && buf[0] != 'S')
            break;
    }

    printf("\nSimulador encerrado.\n");
    return 0;
}
