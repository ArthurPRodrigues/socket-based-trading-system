# Guia rápido de execução e cenários

Compile na raiz:

```bash
make
```

Abra cinco terminais dentro de `src/`. Em todos os cenários, inicie primeiro o Broker:

```bash
./broker
```

## Cenário 1: sucesso

```bash
./sistema_cotacao 100 20 300
./sistema_risco 100 50
./sistema_compras 100 30 0
./sistema_operacao 1
```

## Cenário 2: TTL excedido

A latência do risco (150 ms) é maior que o TTL da cotação (100 ms):

```bash
./sistema_cotacao 100 10 100
./sistema_risco 100 150
./sistema_compras 100 10 0
./sistema_operacao 1
```

## Cenário 3: falha na compra do ativo 2

O último argumento do sistema de compras força a falha do segundo ativo:

```bash
./sistema_cotacao 100 10 500
./sistema_risco 100 10
./sistema_compras 100 10 1
./sistema_operacao 1
```

Parâmetros:

- `sistema_cotacao <sucesso_%> <latencia_ms> <ttl_ms>`
- `sistema_risco <aprovacao_%> <latencia_ms>`
- `sistema_compras <sucesso_%> <latencia_ms> <falhar_ativo2_0_1>`
- `sistema_operacao <numero_de_ordens>`
