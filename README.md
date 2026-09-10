# Pesquisa binária em arquivo — municípios do IBGE

Pesquisa binária feita **no arquivo binário**, um `fseek` + `fread` por
comparação, comparada com a mesma pesquisa sobre o mesmo conjunto já carregado
em memória.

## Compilar e rodar

```
make
./municipios
```

Na primeira execução o `municipios.bin` é gerado a partir do `municipios.txt`.

## Os dados

`municipios.txt` — 5.571 municípios da API de localidades do IBGE
(`servicodados.ibge.gov.br/api/v1/localidades/municipios`), um por linha, no
formato `nome;UF;código`, **já em ordem alfabética** (item 1). O programa
reordena com `qsort` ao gerar o binário: a pesquisa binária depende dessa
invariante e não custa nada garanti-la.

## O registro (tamanho fixo, 88 bytes)

```c
typedef struct {
  char chave[40];   /* "SAO JOAO DEL REI" — ordena e compara */
  char nome[40];    /* "São João del Rei" — exibe            */
  char uf[4];       /* "MG"                                  */
  int  codigo;      /* 3162500                               */
} Municipio;
```

O offset de um registro é `i * sizeof(Municipio)`, e é isso que torna a
pesquisa binária possível sem ler o arquivo inteiro.

**Por que dois campos de nome:** 2.385 dos 5.571 nomes têm acento, e `strcmp`
sobre UTF-8 ordenaria `São Paulo` depois de `Zortéa` (o byte `0xC3` é maior que
qualquer letra ASCII). A `chave` é o nome sem acento em maiúsculas — é ela que
ordena o arquivo e que a busca compara. O `nome` guarda o original para exibir.
De brinde, a busca fica indiferente a acento e caixa: `são joão del rei`,
`SAO JOAO DEL REI` e `Sao Joao Del Rei` chegam no mesmo registro.

**Homônimos:** 232 nomes se repetem em UFs diferentes (`Bom Jesus` aparece 5
vezes). O desempate na ordenação é a UF, então as repetições ficam contíguas no
arquivo; ao encontrar a chave, o programa varre as vizinhas e lista o grupo
inteiro.

## Menu

| | |
|---|---|
| 1 | regravar `municipios.bin` a partir do `.txt` (ordena e grava) |
| 2 | pesquisa binária **no arquivo** |
| 3 | pesquisa binária **em memória** |
| 4 | comparativo arquivo × memória |
| 5 | listar uma faixa de registros |

As opções 2 e 3 imprimem cada registro comparado, o total de comparações e o
tempo. A opção 3 carrega os 5.571 registros de uma vez na primeira chamada.

## Sobre a medição

Cada busca é executada três vezes, com propósitos diferentes:

1. **cronometrada, sem instrumentação** — é o tempo relatado. Os `printf` da
   trilha custam mais que a busca inteira e não podem estar dentro do relógio.
2. **instrumentada** — grava os registros comparados para imprimir depois.
3. **10.000 repetições** — média, na opção 4. Uma busca leva microssegundos e o
   valor de uma execução isolada oscila mais que a diferença que se quer medir.

No comparativo, o tempo de 1× do arquivo é medido **antes** de carregar o
vetor: depois da carga o binário inteiro está no cache do sistema operacional e
o número deixaria de dizer qualquer coisa sobre o disco.

## Resultado

Máximo de 13 comparações para 5.571 registros (`ceil(log2 5572)`), idêntico nos
dois casos — mesmo algoritmo, mesma sequência de posições. O que muda é o custo
de cada comparação:

```
  "ZORTEA" encontrado no registro 5570
              comparacoes   tempo 1x (us)  media de 10000x (us)
  ARQUIVO              13          49.895                 5.600
  MEMORIA              13           0.727                 0.034

  arquivo: 13 fseek + 13 fread de 88 bytes
  memoria: 13 acessos ao vetor, apos ler os 5571 registros de uma vez
  memoria 165x mais rapida na media
```

A memória fica ~70× a ~165× mais rápida — e esse número é o **piso** da
diferença, porque o `fread` sai do buffer do `stdio` e do cache do sistema, não
do disco. Num arquivo grande o bastante para não caber em cache, cada uma das
13 comparações viraria um acesso real ao disco.

O ponto a favor do arquivo é o que ele não paga: as 13 leituras tocam 1.144 dos
490.248 bytes do arquivo (0,2%). A versão em memória precisou ler os 490.248 —
para uma única consulta, a busca em disco termina antes de a busca em memória
ter acabado de carregar.
