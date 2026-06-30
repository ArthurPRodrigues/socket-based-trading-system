CC     = gcc
CFLAGS = -Wall

COMMON = saga.c circuit_breaker.c broker_client.c

all: broker sistema_cotacao sistema_risco sistema_compras teste_e2e simulador

broker:
	$(CC) $(CFLAGS) -o broker broker.c -lpthread

sistema_cotacao:
	$(CC) $(CFLAGS) -o sistema_cotacao sistema_cotacao.c

sistema_risco:
	$(CC) $(CFLAGS) -o sistema_risco sistema_risco.c

sistema_compras:
	$(CC) $(CFLAGS) -o sistema_compras sistema_compras.c

teste_e2e:
	$(CC) $(CFLAGS) -o teste_e2e teste_e2e.c $(COMMON)

simulador:
	$(CC) $(CFLAGS) -o simulador simulador.c $(COMMON)

clean:
	rm -f broker sistema_cotacao sistema_risco sistema_compras teste_e2e simulador

.PHONY: all clean
