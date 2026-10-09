#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#define ARQ_TEXTO "municipios.txt"
#define ARQ_BIN "municipios.bin"
#define ARQ_IDX "municipios.idx"

/* mesmo tamanho do nome de proposito: tirar acento so troca 2 bytes por 1,
   entao a chave nunca estoura o campo e nenhuma busca falha por truncamento */
#define TAM_CHAVE 40
#define TAM_NOME 40
#define TAM_UF 4
#define MAX_MUNICIPIOS 6000
#define MAX_PASSOS 64
#define REPETICOES 10000

#define TAM_K 4

typedef struct {
  char chave[TAM_CHAVE];
  char nome[TAM_NOME];
  char uf[TAM_UF];
  int codigo;
} Municipio;

typedef struct {
  int pos;
  int cmp;
  Municipio reg;
} Passo;

typedef struct {
  int encontrado;
  int pos;
  int comparacoes;
  int passos;
  Passo trilha[MAX_PASSOS];
} Busca;

typedef struct {
  char chave[TAM_CHAVE];
  int reg;
} EntradaIndicePrimario;

typedef struct {
  int k; // tamanho do intervalo apontado por cada entrada de indice
  int total; // total de itens ordenados no arquivo
  int n;
  EntradaIndicePrimario *entradas;
} IndicePrimario;

/* U+00C0..U+00FF em UTF-8 sao 0xC3 seguido de um byte 0x80..0xBF, que indexa
   esta tabela direto */
static const char SEM_ACENTO[] =
    "AAAAAAACEEEEIIIIDNOOOOOxOUUUUYPS"
    "AAAAAAACEEEEIIIIDNOOOOOxOUUUUYPY";

void normalizar(char *destino, const char *origem, int tam) {
  const unsigned char *p = (const unsigned char *)origem;
  int i = 0;

  while (*p != '\0' && i < tam - 1) {
    unsigned char c = *p++;
    if (c == 0xC3 && *p >= 0x80 && *p <= 0xBF)
      c = (unsigned char)SEM_ACENTO[*p++ - 0x80];
    destino[i++] = (char)toupper(c);
  }
  destino[i] = '\0';
}

/* printf conta bytes, entao %-Ns desalinha colunas com nome acentuado */
void imprimir_campo(const char *s, int largura) {
  const unsigned char *p = (const unsigned char *)s;
  int colunas = 0;

  while (*p != '\0')
    if ((*p++ & 0xC0) != 0x80) colunas++;

  printf("%s%*s", s, largura > colunas ? largura - colunas : 0, "");
}

double agora_us(void) {
  struct timespec t;

  clock_gettime(CLOCK_MONOTONIC, &t);
  return (double)t.tv_sec * 1e6 + (double)t.tv_nsec / 1e3;
}

int ler_linha_texto(FILE *f, Municipio *m) {
  char linha[256], *uf, *codigo;

  while (fgets(linha, sizeof(linha), f) != NULL) {
    linha[strcspn(linha, "\r\n")] = '\0';
    if (linha[0] == '\0') continue;

    uf = strchr(linha, ';');
    if (uf == NULL) continue;
    *uf++ = '\0';

    codigo = strchr(uf, ';');
    if (codigo == NULL) continue;
    *codigo++ = '\0';

    if (strlen(linha) >= TAM_NOME || strlen(uf) >= TAM_UF) {
      fprintf(stderr, "registro fora do tamanho, ignorado: %s;%s\n", linha, uf);
      continue;
    }

    memset(m, 0, sizeof(*m));
    normalizar(m->chave, linha, TAM_CHAVE);
    strcpy(m->nome, linha);
    strcpy(m->uf, uf);
    m->codigo = atoi(codigo);
    return 1;
  }
  return 0;
}

int comparar_registros(const void *a, const void *b) {
  const Municipio *x = (const Municipio *)a;
  const Municipio *y = (const Municipio *)b;
  int r = strcmp(x->chave, y->chave);

  return r != 0 ? r : strcmp(x->uf, y->uf);
}

int gerar_arqbin(void) {
  Municipio *v, m;
  FILE *f;
  int total = 0;

  v = malloc((size_t)MAX_MUNICIPIOS * sizeof(Municipio));
  if (v == NULL) return -1;

  f = fopen(ARQ_TEXTO, "r");
  if (f == NULL) {
    perror(ARQ_TEXTO);
    free(v);
    return -1;
  }

  while (ler_linha_texto(f, &m)) {
    if (total == MAX_MUNICIPIOS) {
      fprintf(stderr, "%s tem mais de %d municipios\n", ARQ_TEXTO, MAX_MUNICIPIOS);
      fclose(f);
      free(v);
      return -1;
    }
    v[total++] = m;
  }
  fclose(f);

  qsort(v, (size_t)total, sizeof(Municipio), comparar_registros);

  /* "wb" e nao "w": em modo texto o Windows converte 0x0A em 0x0D 0x0A e
     desalinha todos os registros seguintes */
  f = fopen(ARQ_BIN, "wb");
  if (f == NULL) {
    perror(ARQ_BIN);
    free(v);
    return -1;
  }
  if ((int)fwrite(v, sizeof(Municipio), (size_t)total, f) != total) {
    perror(ARQ_BIN);
    fclose(f);
    free(v);
    return -1;
  }
  fclose(f);
  free(v);

  return total;
}

int total_registros(FILE *f) {
  long bytes;

  if (fseek(f, 0, SEEK_END) != 0) return -1;
  bytes = ftell(f);
  if (bytes < 0) return -1;
  return (int)(bytes / (long)sizeof(Municipio));
}

int ler_registro(FILE *f, int i, Municipio *destino) {
  if (fseek(f, (long)i * (long)sizeof(Municipio), SEEK_SET) != 0) return 0;
  return fread(destino, sizeof(Municipio), 1, f) == 1;
}

Municipio *carregar_memoria(FILE *f, int total) {
  Municipio *v = malloc((size_t)total * sizeof(Municipio));

  if (v == NULL) return NULL;
  if (fseek(f, 0, SEEK_SET) != 0 ||
      (int)fread(v, sizeof(Municipio), (size_t)total, f) != total) {
    free(v);
    return NULL;
  }
  return v;
}

int criar_indice_primario(FILE *f, int total, int k, IndicePrimario *ind) {
  ind->total = total;
  ind->k = k;
  ind->n = (total + k -1) / k;

  ind->entradas = malloc((size_t)ind->n * sizeof(EntradaIndicePrimario));
  if (ind->entradas == NULL) return 0;

  int i;
  for (i = 0; i < ind->n; i++) {
    Municipio reg;
    if (!ler_registro(f, i * k, &reg)) {
      free(ind->entradas);
      ind->entradas = NULL;
      return 0;
    }
    EntradaIndicePrimario ent;
    memcpy(ent.chave, reg.chave, TAM_CHAVE);
    ent.reg = i * k;
    ind->entradas[i] = ent;
  }

  return 1;
}

int gravar_indice_primario(const IndicePrimario *ind) {
  return 0;
}

int carregar_indice_primario(FILE *f, int total, int k, IndicePrimario *ind) {
  return 0;
}

int pesquisar_indice_primario(FILE *f, const IndicePrimario *ind, const char *chave) {
  return -1;
}

void imprimir_indice(const IndicePrimario *ind) {
}

void registrar_passo(Busca *b, int pos, const Municipio *m, int cmp) {
  if (b->passos >= MAX_PASSOS) return;
  b->trilha[b->passos].pos = pos;
  b->trilha[b->passos].cmp = cmp;
  b->trilha[b->passos].reg = *m;
  b->passos++;
}

int busca_arquivo(FILE *f, int total, const char *chave, Busca *b, int trilhar) {
  Municipio m;
  int lo = 0, hi = total - 1;

  b->comparacoes = 0;
  b->passos = 0;
  b->encontrado = 0;

  while (lo <= hi) {
    int mid = lo + (hi - lo) / 2; /* (lo + hi) / 2 estouraria o int */
    int cmp;

    if (!ler_registro(f, mid, &m)) return 0;
    cmp = strcmp(m.chave, chave);
    b->comparacoes++;
    if (trilhar) registrar_passo(b, mid, &m, cmp);

    if (cmp == 0) {
      b->pos = mid;
      b->encontrado = 1;
      return 1;
    }
    if (cmp < 0) lo = mid + 1;
    else hi = mid - 1;
  }

  b->pos = lo;
  return 1;
}

int busca_memoria(const Municipio *v, int total, const char *chave, Busca *b, int trilhar) {
  int lo = 0, hi = total - 1;

  b->comparacoes = 0;
  b->passos = 0;
  b->encontrado = 0;

  while (lo <= hi) {
    int mid = lo + (hi - lo) / 2;
    int cmp = strcmp(v[mid].chave, chave);

    b->comparacoes++;
    if (trilhar) registrar_passo(b, mid, &v[mid], cmp);

    if (cmp == 0) {
      b->pos = mid;
      b->encontrado = 1;
      return 1;
    }
    if (cmp < 0) lo = mid + 1;
    else hi = mid - 1;
  }

  b->pos = lo;
  return 1;
}

void imprimir_trilha(const Busca *b) {
  char rotulo[TAM_NOME + TAM_UF + 2];
  int i;

  printf("\n  passo    reg  municipio comparado                   resultado\n");
  for (i = 0; i < b->passos; i++) {
    const Passo *p = &b->trilha[i];

    snprintf(rotulo, sizeof(rotulo), "%s/%s", p->reg.nome, p->reg.uf);
    printf("   [%2d] %6d  ", i + 1, p->pos);
    imprimir_campo(rotulo, 38);
    if (p->cmp == 0) printf("igual, achou\n");
    else if (p->cmp < 0) printf("lido < alvo, vai para a metade direita\n");
    else printf("lido > alvo, vai para a metade esquerda\n");
  }
}

void listar_homonimos(FILE *f, int total, int pos, const char *chave) {
  Municipio m;
  int i = pos;

  while (i > 0 && ler_registro(f, i - 1, &m) && strcmp(m.chave, chave) == 0) i--;

  for (; i < total && ler_registro(f, i, &m) && strcmp(m.chave, chave) == 0; i++) {
    printf("    reg %6d  byte %9ld  ", i, (long)i * (long)sizeof(Municipio));
    imprimir_campo(m.nome, 34);
    printf("%-3s  cod. %d\n", m.uf, m.codigo);
  }
}

int ler_texto(const char *rotulo, char *destino, int tam) {
  char buffer[256];

  printf("%s", rotulo);
  fflush(stdout);
  if (fgets(buffer, sizeof(buffer), stdin) == NULL) return 0;

  buffer[strcspn(buffer, "\r\n")] = '\0';
  if (buffer[0] == '\0') return 0;

  strncpy(destino, buffer, tam - 1);
  destino[tam - 1] = '\0';
  return 1;
}

int ler_chave(char *chave) {
  char digitado[256];

  if (!ler_texto("municipio: ", digitado, sizeof(digitado))) return 0;
  normalizar(chave, digitado, TAM_CHAVE);
  return 1;
}

void relatar(const Busca *b, double us) {
  if (b->encontrado)
    printf("\n  ENCONTRADO no registro %d apos %d comparacoes (%.3f us)\n",
           b->pos, b->comparacoes, us);
  else
    printf("\n  NAO ENCONTRADO apos %d comparacoes (%.3f us) -- entraria no registro %d\n",
           b->comparacoes, us, b->pos);
}

void buscar_no_arquivo(FILE *f, int total) {
  char chave[TAM_CHAVE];
  Busca b;
  double t0, us;

  if (!ler_chave(chave)) return;

  t0 = agora_us();
  if (!busca_arquivo(f, total, chave, &b, 0)) {
    printf("  erro de leitura em %s\n", ARQ_BIN);
    return;
  }
  us = agora_us() - t0;

  busca_arquivo(f, total, chave, &b, 1);
  imprimir_trilha(&b);
  relatar(&b, us);
  if (b.encontrado) listar_homonimos(f, total, b.pos, chave);
}

void buscar_na_memoria(const Municipio *v, int total) {
  char chave[TAM_CHAVE];
  Busca b;
  double t0, us;
  int i;

  if (!ler_chave(chave)) return;

  t0 = agora_us();
  busca_memoria(v, total, chave, &b, 0);
  us = agora_us() - t0;

  busca_memoria(v, total, chave, &b, 1);
  imprimir_trilha(&b);
  relatar(&b, us);

  if (!b.encontrado) return;
  i = b.pos;
  while (i > 0 && strcmp(v[i - 1].chave, chave) == 0) i--;
  for (; i < total && strcmp(v[i].chave, chave) == 0; i++) {
    printf("    reg %6d  ", i);
    imprimir_campo(v[i].nome, 34);
    printf("%-3s  cod. %d\n", v[i].uf, v[i].codigo);
  }
}

double cronometrar_arquivo(FILE *f, int total, const char *chave) {
  Busca b;
  double t0 = agora_us();
  int i;

  for (i = 0; i < REPETICOES; i++) busca_arquivo(f, total, chave, &b, 0);
  return (agora_us() - t0) / REPETICOES;
}

double cronometrar_memoria(const Municipio *v, int total, const char *chave) {
  Busca b;
  double t0 = agora_us();
  int i;

  for (i = 0; i < REPETICOES; i++) busca_memoria(v, total, chave, &b, 0);
  return (agora_us() - t0) / REPETICOES;
}

void comparar_arquivo_memoria(FILE *f, int total, Municipio **v) {
  char chave[TAM_CHAVE], rotulo[32];
  Busca arq, mem;
  double t0, arq_1x, mem_1x, arq_med, mem_med;

  if (!ler_chave(chave)) return;

  /* o 1x do arquivo tem de vir antes da carga do vetor: depois dela o binario
     inteiro esta no cache do sistema e o numero deixa de medir o disco */
  t0 = agora_us();
  if (!busca_arquivo(f, total, chave, &arq, 0)) {
    printf("  erro de leitura em %s\n", ARQ_BIN);
    return;
  }
  arq_1x = agora_us() - t0;

  if (*v == NULL) {
    *v = carregar_memoria(f, total);
    if (*v == NULL) {
      printf("  memoria insuficiente para carregar %d registros\n", total);
      return;
    }
  }

  t0 = agora_us();
  busca_memoria(*v, total, chave, &mem, 0);
  mem_1x = agora_us() - t0;

  arq_med = cronometrar_arquivo(f, total, chave);
  mem_med = cronometrar_memoria(*v, total, chave);

  snprintf(rotulo, sizeof(rotulo), "media de %dx (us)", REPETICOES);
  printf("\n  \"%s\" %s registro %d\n", chave,
         arq.encontrado ? "encontrado no" : "ausente, entraria no", arq.pos);
  printf("  %-10s %11s %15s %21s\n", "", "comparacoes", "tempo 1x (us)", rotulo);
  printf("  %-10s %11d %15.3f %21.3f\n", "ARQUIVO", arq.comparacoes, arq_1x, arq_med);
  printf("  %-10s %11d %15.3f %21.3f\n", "MEMORIA", mem.comparacoes, mem_1x, mem_med);
  printf("\n  arquivo: %d fseek + %d fread de %zu bytes\n",
         arq.comparacoes, arq.comparacoes, sizeof(Municipio));
  printf("  memoria: %d acessos ao vetor, apos ler os %d registros de uma vez\n",
         mem.comparacoes, total);
  printf("  memoria %.0fx mais rapida na media\n", arq_med / mem_med);
}

void listar_faixa(FILE *f, int total) {
  Municipio m;
  char entrada[32];
  int inicio, quantos, i;

  if (!ler_texto("registro inicial: ", entrada, sizeof(entrada))) return;
  inicio = atoi(entrada);
  if (!ler_texto("quantos: ", entrada, sizeof(entrada))) return;
  quantos = atoi(entrada);

  if (inicio < 0) inicio = 0;
  if (inicio + quantos > total) quantos = total - inicio;

  for (i = inicio; i < inicio + quantos; i++) {
    if (!ler_registro(f, i, &m)) return;
    printf("  %6d  ", i);
    imprimir_campo(m.nome, 34);
    printf("%-3s  cod. %d\n", m.uf, m.codigo);
  }
}

FILE *abrir_arqbin(void) {
  FILE *f = fopen(ARQ_BIN, "rb");

  if (f == NULL) {
    printf("%s nao existe, gerando a partir de %s\n", ARQ_BIN, ARQ_TEXTO);
    if (gerar_arqbin() < 0) return NULL;
    f = fopen(ARQ_BIN, "rb");
    if (f == NULL) perror(ARQ_BIN);
  }
  return f;
}

/* a posicao no vetor e o numero da opcao */
static const char *OPCOES[] = {
    "sair",
    "regravar " ARQ_BIN " a partir de " ARQ_TEXTO,
    "buscar no arquivo (pesquisa binaria em disco)",
    "buscar na memoria (pesquisa binaria em vetor)",
    "comparar arquivo x memoria",
    "listar faixa de registros",
};
#define N_OPCOES (int)(sizeof(OPCOES) / sizeof(OPCOES[0]))

void menu(void) {
  printf("\n");
  for (int i = 1; i < N_OPCOES; i++) printf("%d - %s\n", i, OPCOES[i]);
  printf("0 - %s\n", OPCOES[0]);
}

int ler_opcao(void) {
  char linha[32];
  char *fim;
  long opcao;

  if (!ler_texto("opcao: ", linha, sizeof(linha))) return 0;
  opcao = strtol(linha, &fim, 10);
  if (fim == linha || *fim != '\0' || opcao < 0 || opcao >= N_OPCOES) return -1;
  return (int)opcao;
}

int main(void) {
  FILE *bin = abrir_arqbin();
  Municipio *v = NULL;
  int total, opcao;

  if (bin == NULL) return 1;

  total = total_registros(bin);
  printf("%d municipios em %s, registros de %zu bytes (%ld bytes no total)\n",
         total, ARQ_BIN, sizeof(Municipio), (long)total * (long)sizeof(Municipio));

  while (menu(), (opcao = ler_opcao()) != 0) {
    switch (opcao) {
      case 1:
        fclose(bin);
        free(v);
        v = NULL;
        if (gerar_arqbin() < 0) return 1;
        bin = abrir_arqbin();
        if (bin == NULL) return 1;
        total = total_registros(bin);
        printf("  %d municipios regravados\n", total);
        break;
      case 2:
        buscar_no_arquivo(bin, total);
        break;
      case 3:
        if (v == NULL) {
          v = carregar_memoria(bin, total);
          if (v == NULL) {
            printf("  memoria insuficiente para carregar %d registros\n", total);
            break;
          }
          printf("  %d registros carregados em memoria\n", total);
        }
        buscar_na_memoria(v, total);
        break;
      case 4:
        comparar_arquivo_memoria(bin, total, &v);
        break;
      case 5:
        listar_faixa(bin, total);
        break;
      default:
        printf("  opcao invalida\n");
    }
  }

  free(v);
  fclose(bin);
  return 0;
}
