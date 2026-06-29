# Instruções de compilação e execução

## Pré-requisitos

- GCC
- Make (opcional)
- Sistema operacional Linux/macOS

---

## Compilação

Todos os comandos devem ser executados dentro do diretório `src/`.

```bash
cd src/
```

### Serviços

```bash
gcc -o broker          broker.c          -lpthread
gcc -o sistema_cotacao sistema_cotacao.c
gcc -o sistema_risco   sistema_risco.c
gcc -o sistema_compras sistema_compras.c
```

### Orquestrador (teste automatizado)

```bash
gcc -o teste_e2e teste_e2e.c saga.c circuit_breaker.c broker_client.c
```

### Simulador interativo

```bash
gcc -o simulador simulador.c saga.c circuit_breaker.c broker_client.c
```

---

## Execução

### Teste automatizado (3 cenários fixos)

Executa em sequência os cenários 1, 2 e 3 com parâmetros predefinidos:

```bash
cd src/
./teste_e2e
```

### Simulador interativo

Permite configurar manualmente os parâmetros de cada serviço antes de cada execução:

```bash
cd src/
./simulador
```

O simulador solicita, para cada rodada:

| Campo | Descrição |
|---|---|
| Cotação — Sleep (ms) | Latência simulada da cotação |
| Cotação — Taxa de sucesso (%) | Probabilidade de a cotação responder |
| Risco — Sleep (ms) | Latência simulada da análise de risco |
| Risco — Taxa de sucesso (%) | Probabilidade de aprovação pelo risco |
| Compras — Sleep (ms) | Latência simulada de cada compra |
| Compras — Taxa de sucesso (%) | Probabilidade de sucesso de cada compra |
| Compras — Forçar falha na chamada N | Força falha na N-ésima compra (-1 = desativado) |
| Ativo 1 | Par de mercado (ex: `ETH/USDT`) |
| Ativo 2 | Par de mercado (ex: `USD/BRL`) |
| TTL (ms) | Tempo de vida da cotação em milissegundos |

Ao final de cada execução, o simulador pergunta se deseja rodar outro cenário.

---

## Exemplos de cenários

### Cenário 1 — Sucesso completo

Todas as integrações funcionam sem falhas ou atrasos.

```
Cotação:  sleep=0ms,   taxa=100%
Risco:    sleep=0ms,   taxa=100%
Compras:  sleep=0ms,   taxa=100%,  fail-on=-1
Ativo 1:  ETH/USDT
Ativo 2:  USD/BRL
TTL:      300ms
```

### Cenário 2 — Falha na compra do Ativo 2

A segunda chamada ao sistema de compras falha, acionando o rollback do Ativo 1.

```
Cotação:  sleep=0ms,   taxa=100%
Risco:    sleep=0ms,   taxa=100%
Compras:  sleep=0ms,   taxa=100%,  fail-on=2
Ativo 1:  ETH/USDT
Ativo 2:  USD/BRL
TTL:      300ms
```

### Cenário 3 — TTL excedido

A cotação demora mais do que o TTL permitido, abortando a operação antes de prosseguir.

```
Cotação:  sleep=400ms, taxa=100%
Risco:    sleep=0ms,   taxa=100%
Compras:  sleep=0ms,   taxa=100%,  fail-on=-1
Ativo 1:  ETH/USDT
Ativo 2:  USD/BRL
TTL:      300ms
```
