# TDE - Índice primário

## 1. Cada intervalo tem quantos registros?

46 registros (`TAM_K`). Como 46 x 88 = 4048 bytes, cada intervalo cabe numa página de 4 KB.

São 5571 registros, então o índice tem 122 entradas. O último intervalo tem só 5 registros.

## 2. Utilizou pesquisa binária no índice?

Pesquisa (opção 6 do menu):

```
municipio: novo horizonte

  ENCONTRADO no registro 3402, intervalo da entrada 73
    reg   3402  byte    299376  Novo Horizonte                    BA   cod. 2923035
    reg   3403  byte    299464  Novo Horizonte                    SC   cod. 4211652
    reg   3404  byte    299552  Novo Horizonte                    SP   cod. 3533502
```

Sim. O índice foi montado a partir do arquivo, que já está ordenado, então as entradas também ficam em ordem. A pesquisa binária no índice acha o intervalo certo. Depois, o programa lê só esse intervalo do arquivo e procura o registro dentro dele.

## Sobre o código

Esta tarefa foi feita em cima do código de um trabalho anterior (pesquisa binária em arquivo), por isso o `municipios.c` tem outras funções. As partes novas, que importam aqui, são:

- `EntradaIndicePrimario` e `IndicePrimario`: as structs do índice;
- `criar_indice_primario`: monta o índice lendo um registro a cada K;
- `gravar_indice_primario` e `carregar_indice_primario`: salvam o índice em `municipios.idx` e leem de volta ao reiniciar (e recriam se o arquivo não bater com o `.bin`);
- `pesquisar_indice_primario`: pesquisa binária no índice e leitura do intervalo;
- `imprimir_indice` e `buscar_pelo_indice`: opções 7 e 6 do menu.

## Uso de IA

Usei o Claude Opus 5.5 da Anthropic como apoio ao longo do trabalho, principalmente nestas partes:

- Discussão do formato do índice e do arquivo `.idx`;
- Revisão e correção das funções de criar, gravar e carregar o índice;
- Escrita da função de pesquisa pelo índice, da impressão e das opções no menu;
- Testes da pesquisa com todas as chaves do arquivo.

As decisões de estrutura e de funcionamento do índice foram minhas; a IA funcionou como par de programação para escrever, revisar e testar trechos de código C a partir dessas decisões.
