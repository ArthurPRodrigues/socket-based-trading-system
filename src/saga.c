#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/time.h>
#include <time.h>

// Vamos incluir o cabeçalho do nosso futuro gateway de comunicação
#include "gateway.h"

// Portas dos sistemas 
#define PORTA_COTACAO 8081
#define PORTA_RISCO   8082
#define PORTA_COMPRAS 8083

// Função auxiliar para pegar o tempo atual em milissegundos
long long obter_tempo_ms() {
    struct timeval tv;
    gettimeofday(&tv, NULL);
    return (((long long)tv.tv_sec) * 1000) + (tv.tv_usec / 1000);
}

//O Padrão Saga
void executar_saga_trading(const char *ativo1, const char *ativo2, int ttl_max_ms) {
    long long tempo_inicio = obter_tempo_ms();
    int ativo1_comprado = 0; // Flag de estado para saber se precisamos fazer rollback
    char buffer_envio[512];
    char buffer_resposta[256];

    // Gerando um ID único para a transação (Padrão de Idempotência)
    srand(time(NULL));
    int id_ordem = rand() % 90000 + 10000; 

    printf("\n⚡ [SAGA %d] Iniciando ordem para %s e %s...\n", id_ordem, ativo1, ativo2);

    // ==========================================
    // PASSO 1: SISTEMA DE COTAÇÃO
    // ==========================================
    if ((obter_tempo_ms() - tempo_inicio) > ttl_max_ms) {
        printf("❌ [ABORTADO] TTL excedido antes da Cotação.\n");
        return;
    }
    
    sprintf(buffer_envio, "ID:%d;CMD:COTAR;ATIVOS:%s,%s", id_ordem, ativo1, ativo2);
    if (enviar_mensagem(PORTA_COTACAO, buffer_envio, buffer_resposta, sizeof(buffer_resposta)) != 0) {
        printf("❌ [ABORTADO] Falha de comunicação com o sistema de Cotação.\n");
        return;
    }
    printf("✅ [1/4] Cotação recebida: %s\n", buffer_resposta);

    // ==========================================
    // PASSO 2: SISTEMA DE RISCO
    // ==========================================
    if ((obter_tempo_ms() - tempo_inicio) > ttl_max_ms) {
        printf("❌ [ABORTADO] TTL excedido antes do Risco.\n");
        return;
    }

    sprintf(buffer_envio, "ID:%d;CMD:AVALIAR;DADOS:%s", id_ordem, buffer_resposta);
    if (enviar_mensagem(PORTA_RISCO, buffer_envio, buffer_resposta, sizeof(buffer_resposta)) != 0 || 
        strcmp(buffer_resposta, "APROVADO") != 0) {
        printf("❌ [ABORTADO] Operação reprovada pelo Risco.\n");
        return;
    }
    printf("✅ [2/4] Risco Aprovado.\n");

    // ==========================================
    // PASSO 3: COMPRA DO ATIVO 1
    // ==========================================
    if ((obter_tempo_ms() - tempo_inicio) > ttl_max_ms) {
        printf("[ABORTADO] TTL excedido antes de comprar o Ativo 1.\n");
        return;
    }

    sprintf(buffer_envio, "ID:%d;CMD:COMPRAR;ATIVO:%s", id_ordem, ativo1);
    if (enviar_mensagem(PORTA_COMPRAS, buffer_envio, buffer_resposta, sizeof(buffer_resposta)) == 0 && 
        strcmp(buffer_resposta, "SUCESSO") == 0) {
        ativo1_comprado = 1; // Marcamos que compramos! Se der erro daqui pra frente, tem que vender.
        printf("[3/4] Ativo 1 (%s) comprado com sucesso.\n", ativo1);
    } else {
        printf("[ABORTADO] Falha na compra do Ativo 1.\n");
        return;
    }

    // ==========================================
    // PASSO 4: COMPRA DO ATIVO 2
    // ==========================================
    if ((obter_tempo_ms() - tempo_inicio) > ttl_max_ms) {
        printf("[FALHA] TTL excedido antes do Ativo 2! Iniciando Rollback...\n");
        goto compensacao; // Pula direto para a lógica de desfazer
    }

    sprintf(buffer_envio, "ID:%d;CMD:COMPRAR;ATIVO:%s", id_ordem, ativo2);
    if (enviar_mensagem(PORTA_COMPRAS, buffer_envio, buffer_resposta, sizeof(buffer_resposta)) == 0 && 
        strcmp(buffer_resposta, "SUCESSO") == 0) {
        printf("[4/4 SUCESSO] Saga concluída! %s e %s adquiridos.\n", ativo1, ativo2);
        return; // Sucesso total, encerra a função.
    } else {
        printf("[FALHA] Erro na compra do Ativo 2! Iniciando Rollback...\n");
    }

// ==========================================
// TRANSAÇÃO DE COMPENSAÇÃO (ROLLBACK)
// ==========================================
// Se o código chegou aqui, é porque o Ativo 2 falhou ou o TTL estourou no final.
compensacao:
    if (ativo1_comprado) {
        sprintf(buffer_envio, "ID:%d;CMD:DESFAZER;ATIVO:%s", id_ordem, ativo1);
        enviar_mensagem(PORTA_COMPRAS, buffer_envio, buffer_resposta, sizeof(buffer_resposta));
        printf("[ROLLBACK] Ativo 1 (%s) vendido/desfeito para compensar a falha.\n", ativo1);
    }
    printf("[SAGA %d] Operação inteiramente cancelada de forma atômica.\n", id_ordem);
}