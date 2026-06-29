CC = gcc
CFLAGS = -std=c11 -Wall -Wextra -Wpedantic -D_DEFAULT_SOURCE
LDFLAGS =
SRC = src

.PHONY: all clean

all: broker sistema_operacao sistema_cotacao sistema_risco sistema_compras teste_cb

broker: $(SRC)/broker.c $(SRC)/broker.h
	$(CC) $(CFLAGS) $(SRC)/broker.c -o $(SRC)/broker -pthread

sistema_operacao: $(SRC)/main.c $(SRC)/saga.c $(SRC)/saga.h $(SRC)/circuit_breaker.c $(SRC)/circuit_breaker.h $(SRC)/broker_client.c $(SRC)/broker_client.h $(SRC)/broker.h
	$(CC) $(CFLAGS) $(SRC)/main.c $(SRC)/saga.c $(SRC)/circuit_breaker.c $(SRC)/broker_client.c -o $(SRC)/sistema_operacao

sistema_cotacao: $(SRC)/sistema_cotacao.c
	$(CC) $(CFLAGS) $(SRC)/sistema_cotacao.c -o $(SRC)/sistema_cotacao

sistema_risco: $(SRC)/sistema_risco.c
	$(CC) $(CFLAGS) $(SRC)/sistema_risco.c -o $(SRC)/sistema_risco

sistema_compras: $(SRC)/sistema_compras.c
	$(CC) $(CFLAGS) $(SRC)/sistema_compras.c -o $(SRC)/sistema_compras

teste_cb: $(SRC)/teste_cb.c $(SRC)/circuit_breaker.c $(SRC)/broker_client.c
	$(CC) $(CFLAGS) $(SRC)/teste_cb.c $(SRC)/circuit_breaker.c $(SRC)/broker_client.c -o $(SRC)/teste_cb

clean:
	rm -f $(SRC)/broker $(SRC)/sistema_operacao $(SRC)/sistema_cotacao \
	      $(SRC)/sistema_risco $(SRC)/sistema_compras $(SRC)/teste_cb $(SRC)/*.o
