# Sistema de Operação em Mercado (Trading) Distribuído

Este projeto foi desenvolvido como parte da disciplina **INE 5645 - Programação Paralela e Distribuída** da UFSC, semestre 2026/1. O objetivo é a implementação de um protótipo de sistema de trading automatizado, utilizando programação distribuída e troca de mensagens via *sockets Berkeley*.

## Sobre o Projeto

O sistema simula o fluxo de compra de dois ativos, integrando quatro processos independentes que se comunicam para garantir a execução confiável e atômica da operação:

1. **Sistema de Operação:** Coordena o fluxo, gera a ordem e gerencia o estado da transação.
2. **Sistema de Cotação:** Fornece valores dos ativos com um TTL (Time-To-Live) estrito.
3. **Sistema de Risco:** Avalia a viabilidade da operação.
4. **Sistema de Compras:** Realiza a execução efetiva da compra.

## Funcionalidades

* **Comunicação via Sockets:** Implementação baseada em sockets Berkeley para intercâmbio de mensagens entre processos.
* **Resiliência:** Tratamento de falhas, incluindo estouro de TTL e falha na integração de ativos, com cancelamento atômico da operação (rollback).
* **Configurabilidade:** Parâmetros ajustáveis de taxa de sucesso e tempo de processamento (*sleep*) para simulação de cenários reais de rede e carga.
* **Padrões de Projeto:** Aplicação de padrões de projeto para sistemas distribuídos nas integrações.

## Integrantes

- Arthur Paulo Rodrigues (23100747)
- Adan Samuel Prüss (2410089)
- Roberto Gabriel Ferreira (23100739)
