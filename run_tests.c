#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <limits.h>

#include "sbas.h"

#define VERDE "\033[0;32m"
#define RESETAR_COR "\033[0m"

static void roda_teste_parseia_toda_a_gramatica();
static void roda_teste_retorno_zero();
static void roda_teste_retorno_constante();
static void roda_teste_retorno_variavel();
static void roda_teste_overflow();
static void roda_teste_atribuicao_de_constante();
static void roda_teste_atribuicao_de_parametro();
static void roda_teste_atribuicao_de_variavel();
static void roda_teste_todos_casos_operacao_aritmetica();
static void roda_teste_operacao_aritmetica();
static void roda_teste_soma_um_ao_argumento();
static void roda_teste_salva_callee_saveds();
static void roda_teste_diferenca_de_quadrados();
static void roda_teste_eh_negativo();
static void roda_teste_fatorial();
static void roda_teste_retorno_de_um_param();
static void roda_teste_dead_code();
static void roda_teste_multiplos_branches();
static void roda_teste_multiplos_ifs_encadeados();
static void roda_teste_dois_argumentos();
static void roda_teste_tres_argumentos();
static void roda_teste_multiplica_param_por_dez();
static void roda_teste_multiplica();
static void roda_teste_subtracao_1();
static void roda_teste_subtracao_2();

int main(void) {
  roda_teste_parseia_toda_a_gramatica();
  roda_teste_salva_callee_saveds();
  roda_teste_retorno_zero();
  roda_teste_retorno_de_um_param();
  roda_teste_retorno_constante();
  roda_teste_retorno_variavel();
  roda_teste_overflow();
  roda_teste_atribuicao_de_constante();
  roda_teste_atribuicao_de_parametro();
  roda_teste_atribuicao_de_variavel();
  roda_teste_todos_casos_operacao_aritmetica();
  roda_teste_soma_um_ao_argumento();
  roda_teste_operacao_aritmetica();
  roda_teste_diferenca_de_quadrados();
  roda_teste_fatorial();
  roda_teste_eh_negativo();
  roda_teste_dead_code();
  roda_teste_multiplos_branches();
  roda_teste_multiplos_ifs_encadeados();
  roda_teste_dois_argumentos();
  roda_teste_tres_argumentos();
  roda_teste_multiplica_param_por_dez();
  roda_teste_multiplica();
  roda_teste_subtracao_1();
  roda_teste_subtracao_2();

  return 0;
}

static void roda_teste_parseia_toda_a_gramatica() {
  FILE* arquivoSbas;
  unsigned char codigo[500];
  funcp funcaoSBas;

  arquivoSbas = fopen("test_files/tudo.sbas", "r");
  if (!arquivoSbas) {
    printf("Falha ao abrir arquivo SBas para leitura!\n");
    return;
  }
  printf("Testando se toda a gramática é parseada corretamente. Arquivo: tudo.sbas\n");

  // Gera a função
  funcaoSBas = peqcomp(arquivoSbas, codigo);

  // O ponteiro para esta não deve ser NULL
  assert(funcaoSBas != NULL);

  fclose(arquivoSbas);
  printf(VERDE "Teste de parse da gramática passou!\n" RESETAR_COR);
}

static void roda_teste_salva_callee_saveds() {
  FILE* arquivoSbas;
  unsigned char codigo[500];
  funcp funcaoSBas;

  arquivoSbas = fopen("test_files/atribuicao_de_variaveis.sbas", "r");
  if (!arquivoSbas) {
    printf("Falha ao abrir arquivo SBas para leitura!\n");
    return;
  }
  printf("Testando se callee-saveds são preservados...\n");

  funcaoSBas = peqcomp(arquivoSbas, codigo);
  assert(funcaoSBas != NULL);

  unsigned long rbx, r12, r13, r14, r15;
  rbx = 0x11111111;
  r12 = 0x22222222;
  r13 = 0x33333333;
  r14 = 0x44444444;
  r15 = 0x55555555;

  asm volatile(
      "mov %[val_rbx], %%rbx\n\t"
      "mov %[val_r12], %%r12\n\t"
      "mov %[val_r13], %%r13\n\t"
      "mov %[val_r14], %%r14\n\t"
      "mov %[val_r15], %%r15\n\t"
      :
      : [val_rbx] "r"(rbx), [val_r12] "r"(r12), [val_r13] "r"(r13), [val_r14] "r"(r14), [val_r15] "r"(r15)
      : "rbx", "r12", "r13", "r14", "r15");

  // Chama a função
  (*funcaoSBas)();

  // Carrega registradores depois do uso da função em variáveis
  unsigned long rbx_depois, r12_depois, r13_depois, r14_depois, r15_depois;
  asm volatile(
      "mov %%rbx, %[out_b]\n\t"
      "mov %%r12, %[out_r12]\n\t"
      "mov %%r13, %[out_r13]\n\t"
      "mov %%r14, %[out_r14]\n\t"
      "mov %%r15, %[out_r15]\n\t"
      : [out_b] "=r"(rbx_depois), [out_r12] "=r"(r12_depois), [out_r13] "=r"(r13_depois), [out_r14] "=r"(r14_depois), [out_r15] "=r"(r15_depois));

  assert(rbx_depois == rbx && "RBX foi alterado e não restaurado pelo código gerado!");
  assert(r12_depois == r12 && "R12 foi alterado e não restaurado pelo código gerado!");
  assert(r13_depois == r13 && "R13 foi alterado e não restaurado pelo código gerado!");
  assert(r14_depois == r14 && "R14 foi alterado e não restaurado pelo código gerado!");
  assert(r15_depois == r15 && "R15 foi alterado e não restaurado pelo código gerado!");

  fclose(arquivoSbas);
  printf(VERDE "Teste de callee-saveds passou!\n" RESETAR_COR);
}

static void roda_teste_retorno_zero() {
  FILE* arquivoSbas;
  unsigned char codigo[500];
  funcp funcaoSBas;
  int resultado;

  arquivoSbas = fopen("test_files/retorno_zero.sbas", "r");
  if (!arquivoSbas) {
    printf("Falha ao abrir arquivo SBas para leitura!\n");
    return;
  }
  printf("Testando retorno de constante zero. Arquivo: retorno_zero.sbas\n");

  // Gera a função
  funcaoSBas = peqcomp(arquivoSbas, codigo);

  // O ponteiro para esta não deve ser NULL
  assert(funcaoSBas != NULL);

  // Obtem o resultado
  resultado = (*funcaoSBas)();

  // Garante resultado esperado
  assert(resultado == 0);

  fclose(arquivoSbas);
  printf(VERDE "Teste de retorno zero passou!\n" RESETAR_COR);
}

static void roda_teste_retorno_constante() {
  FILE* arquivoSbas;
  unsigned char codigo[500];
  funcp funcaoSBas;
  int resultado;

  arquivoSbas = fopen("test_files/retorno_constante.sbas", "r");
  if (!arquivoSbas) {
    printf("Falha ao abrir arquivo SBas para leitura!\n");
    return;
  }
  printf("Testando retorno de constante qualquer. Arquivo: retorno_constante.sbas\n");

  // Gera a função
  funcaoSBas = peqcomp(arquivoSbas, codigo);

  // O ponteiro para esta não deve ser NULL
  assert(funcaoSBas != NULL);

  // Obtem o resultado
  resultado = (*funcaoSBas)();

  // Garante resultado esperado
  assert(resultado == 16909060);

  fclose(arquivoSbas);
  printf(VERDE "Teste de retorno de uma constante passou!\n" RESETAR_COR);
}

static void roda_teste_retorno_de_um_param() {
  FILE* arquivoSbas;
  unsigned char codigo[500];
  funcp funcaoSBas;
  int resultado;

  arquivoSbas = fopen("test_files/retorno_de_um_param.sbas", "r");
  if (!arquivoSbas) {
    printf("Falha ao abrir arquivo SBas para leitura!\n");
    return;
  }
  printf("Testando retorno de um param . Arquivo: retorno_de_um_param.sbas\n");

  // Gera a função
  funcaoSBas = peqcomp(arquivoSbas, codigo);

  // O ponteiro para esta não deve ser NULL
  assert(funcaoSBas != NULL);

  resultado = (*funcaoSBas)(16909060);  
  assert(resultado == 16909060);

  resultado = (*funcaoSBas)(-1253512);  
  assert(resultado == -1253512);

  fclose(arquivoSbas);
  printf(VERDE "Teste de retorno de um param passou!\n" RESETAR_COR);
}

static void roda_teste_retorno_variavel() {
  FILE* arquivoSbas;
  unsigned char codigo[500];
  funcp funcaoSBas;
  int resultado;

  arquivoSbas = fopen("test_files/retorno_variavel.sbas", "r");
  if (!arquivoSbas) {
    printf("Falha ao abrir arquivo SBas para leitura!\n");
    return;
  }
  printf("Testando retorno de uma variável local. Arquivo: retorno_variavel.sbas\n");

  // Gera a função
  funcaoSBas = peqcomp(arquivoSbas, codigo);

  // O ponteiro para esta não deve ser NULL
  assert(funcaoSBas != NULL);

  // Obtem o resultado
  resultado = (*funcaoSBas)();

  // Garante resultado esperado
  assert(resultado == 5);

  fclose(arquivoSbas);
  printf(VERDE "Teste de retorno de variável local passou!\n" RESETAR_COR);
}

static void roda_teste_overflow() {
  FILE* arquivoSbas;
  unsigned char codigo[500];
  funcp funcaoSBas;
  int resultado;

  arquivoSbas = fopen("test_files/overflow_1.sbas", "r");
  if (!arquivoSbas) {
    printf("Falha ao abrir arquivo SBas para leitura!\n");
    return;
  }
  printf("Testando overflow de INT_MAX em INT_MIN. Arquivo: overflow_1.sbas\n");

  // Gera a função
  funcaoSBas = peqcomp(arquivoSbas, codigo);

  // O ponteiro para esta não deve ser NULL
  assert(funcaoSBas != NULL);

  // Obtem o resultado
  resultado = (*funcaoSBas)();

  // Espera que INT_MAX + 1 = INT_MIN
  assert(resultado == -2147483648);

  fclose(arquivoSbas);
  arquivoSbas = fopen("test_files/overflow_2.sbas", "r");
  if (!arquivoSbas) {
    printf("Falha ao abrir arquivo SBas para leitura!\n");
    return;
  }
  printf("Testando overflow de INT_MIN em INT_MAX. Arquivo: overflow_2.sbas\n");

  // Gera a função
  funcaoSBas = peqcomp(arquivoSbas, codigo);

  // O ponteiro para esta não deve ser NULL
  assert(funcaoSBas != NULL);

  // Obtem o resultado
  resultado = (*funcaoSBas)();

  // Espera que INT_MIN - 1 = INT_MAX
  assert(resultado == 2147483647);

  fclose(arquivoSbas);
  printf(VERDE "Teste de overflow passou!\n" RESETAR_COR);
}

static void roda_teste_atribuicao_de_constante() {
  FILE* arquivoSbas;
  unsigned char codigo[500];
  funcp funcaoSBas;
  int resultado;

  arquivoSbas = fopen("test_files/atribuicao_de_constante.sbas", "r");
  if (!arquivoSbas) {
    printf("Falha ao abrir arquivo SBas para leitura!\n");
    return;
  }
  printf("Testando atribuição de constantes. Arquivo: atribuicao_de_constante.sbas\n");

  // Gera a função
  funcaoSBas = peqcomp(arquivoSbas, codigo);

  // O ponteiro para esta não deve ser NULL
  assert(funcaoSBas != NULL);

  resultado = (*funcaoSBas)();
  assert(resultado == -1004);

  fclose(arquivoSbas);
  printf(VERDE "Teste de atribuição de constantes passou!\n" RESETAR_COR);
}

static void roda_teste_atribuicao_de_parametro() {
  FILE* arquivoSbas;
  unsigned char codigo[500];
  funcp funcaoSBas;

  arquivoSbas = fopen("test_files/atribuicao_de_parametros.sbas", "r");
  if (!arquivoSbas) {
    printf("Falha ao abrir arquivo SBas para leitura!\n");
    return;
  }
  printf("Testando atribuição de parâmetros. Arquivo: atribuicao_de_parametros.sbas\n");

  // Gera a função
  funcaoSBas = peqcomp(arquivoSbas, codigo);

  // O ponteiro para esta não deve ser NULL
  assert(funcaoSBas != NULL);

  fclose(arquivoSbas);
  printf(VERDE "Teste de atribuição de parâmetros passou!\n" RESETAR_COR);
}

static void roda_teste_atribuicao_de_variavel() {
  FILE* arquivoSbas;
  unsigned char codigo[500];
  funcp funcaoSBas;

  arquivoSbas = fopen("test_files/atribuicao_de_variaveis.sbas", "r");
  if (!arquivoSbas) {
    printf("Falha ao abrir arquivo SBas para leitura!\n");
    return;
  }
  printf("Testando atribuição de variáveis locais. Arquivo: atribuicao_de_variaveis.sbas\n");

  // Gera a função
  funcaoSBas = peqcomp(arquivoSbas, codigo);

  // O ponteiro para esta não deve ser NULL
  assert(funcaoSBas != NULL);

  fclose(arquivoSbas);
  printf(VERDE "Teste de atribuição de variáveis locais passou!\n" RESETAR_COR);
}

static void roda_teste_todos_casos_operacao_aritmetica() {
  FILE* arquivoSbas;
  unsigned char codigo[500];
  funcp funcaoSBas;
  int resultado;

  arquivoSbas = fopen("test_files/todos_casos_op_aritmetica.sbas", "r");
  if (!arquivoSbas) {
    printf("Falha ao abrir arquivo SBas para leitura!\n");
    return;
  }
  printf("Testando todos os casos de operações aritméticas. Arquivo: todos_casos_op_aritmetica.sbas\n");

  // Gera a função
  funcaoSBas = peqcomp(arquivoSbas, codigo);

  // O ponteiro para esta não deve ser NULL
  assert(funcaoSBas != NULL);

  resultado = funcaoSBas();
  assert(resultado == -746);

  fclose(arquivoSbas);
  printf(VERDE "Teste de todos os casos de operações aritméticas passou!\n" RESETAR_COR);
}

static void roda_teste_operacao_aritmetica() {
  FILE* arquivoSbas;
  unsigned char codigo[500];
  funcp funcaoSBas;
  int resultado;

  arquivoSbas = fopen("test_files/operacao_aritmetica.sbas", "r");
  if (!arquivoSbas) {
    printf("Falha ao abrir arquivo SBas para leitura!\n");
    return;
  }
  printf("Testando operações aritméticas. Arquivo: operacao_aritmetica.sbas\n");

  // Gera a função
  funcaoSBas = peqcomp(arquivoSbas, codigo);

  // O ponteiro para esta não deve ser NULL
  assert(funcaoSBas != NULL);

  resultado = (*funcaoSBas)(3);

  assert(resultado == 2520);

  fclose(arquivoSbas);
  printf(VERDE "Teste de operações aritméticas passou!\n" RESETAR_COR);
}

static void roda_teste_soma_um_ao_argumento() {
  FILE* arquivoSbas;
  unsigned char codigo[500];
  funcp funcaoSBas;
  int resultado;

  arquivoSbas = fopen("test_files/fx_x+1.sbas", "r");
  if (!arquivoSbas) {
    printf("Falha ao abrir arquivo SBas para leitura!\n");
    return;
  }
  printf("Testando somar 1 ao primeiro argumento e retorná-lo. Arquivo: fx_x+1.sbas\n");

  // Gera a função
  funcaoSBas = peqcomp(arquivoSbas, codigo);

  // O ponteiro para esta não deve ser NULL
  assert(funcaoSBas != NULL);

  resultado = funcaoSBas(0);
  assert(resultado == 1);

  resultado = funcaoSBas(-1);
  assert(resultado == 0);

  resultado = funcaoSBas(1);
  assert(resultado == 2);

  resultado = funcaoSBas(-5);
  assert(resultado == -4);

  resultado = funcaoSBas(INT_MAX);
  assert(resultado == INT_MIN);

  resultado = funcaoSBas(INT_MIN - 1);
  assert(resultado == INT_MIN);

  fclose(arquivoSbas);
  printf(VERDE "Teste de somar 1 ao primeiro argumento passou!\n" RESETAR_COR);
}

static void roda_teste_diferenca_de_quadrados() {
  FILE* arquivoSbas;
  unsigned char codigo[500];
  funcp funcaoSBas;

  arquivoSbas = fopen("test_files/diferenca_de_quadrados.sbas", "r");
  if (!arquivoSbas) {
    printf("Falha ao abrir arquivo SBas para leitura!\n");
    return;
  }
  printf("Testando diferença de quadrados. Arquivo: diferenca_de_quadrados.sbas\n");

  funcaoSBas = peqcomp(arquivoSbas, codigo);

  assert(funcaoSBas != NULL);

  assert(funcaoSBas(4, 2) == 12);   // 16 - 4 = 12
  assert(funcaoSBas(5, 0) == 25);   // 25 - 0 = 25
  assert(funcaoSBas(7, 7) == 0);    // 49 - 49 = 0
  assert(funcaoSBas(10, 1) == 99);  // 100 - 1 = 99
  assert(funcaoSBas(-3, 2) == 5);   // 9 - 4 = 5
  assert(funcaoSBas(0, 3) == -9);   // 0 - 9 = -9
  assert(funcaoSBas(-5, -4) == 9);  // 25 - 16 = 9

  fclose(arquivoSbas);
  printf(VERDE "Teste de diferença de quadrados passou!\n" RESETAR_COR);
}

static void roda_teste_eh_negativo() {
  FILE* arquivoSbas;
  unsigned char codigo[500];
  funcp funcaoSBas;
  int resultado;

  arquivoSbas = fopen("test_files/eh_negativo.sbas", "r");
  if (!arquivoSbas) {
    printf("Falha ao abrir arquivo SBas para leitura!\n");
    return;
  }

  printf("Testando se número somado a um é negativo. Arquivo: eh_negativo.sbas\n");

  funcaoSBas = peqcomp(arquivoSbas, codigo);

  assert(funcaoSBas != NULL);

  resultado = funcaoSBas(0);
  assert(resultado == 0);

  resultado = funcaoSBas(-1);
  assert(resultado == 1);

  resultado = funcaoSBas(-2);
  assert(resultado == 1);

  resultado = funcaoSBas(1);
  assert(resultado == 0);

  fclose(arquivoSbas);
  printf(VERDE "Teste de se número somado a um é negativo passou!\n" RESETAR_COR);
}

static void roda_teste_fatorial() {
  FILE* arquivoSbas;
  unsigned char codigo[500];
  funcp funcaoSBas;
  int resultado;

  arquivoSbas = fopen("test_files/fatorial.sbas", "r");
  if (!arquivoSbas) {
    printf("Falha ao abrir arquivo SBas para leitura!\n");
    return;
  }

  printf("Testando fatorial. Arquivo: fatorial.sbas\n");

  funcaoSBas = peqcomp(arquivoSbas, codigo);

  assert(funcaoSBas != NULL);

  resultado = funcaoSBas(0);
  assert(resultado == 1);

  resultado = funcaoSBas(1);
  assert(resultado == 1);

  assert(funcaoSBas(2) == 2);     // 2! = 2
  assert(funcaoSBas(3) == 6);     // 3! = 6
  assert(funcaoSBas(4) == 24);    // 4! = 24
  assert(funcaoSBas(5) == 120);   // 5! = 120

  assert(funcaoSBas(6) == 720);
  assert(funcaoSBas(7) == 5040);
  assert(funcaoSBas(8) == 40320);
  assert(funcaoSBas(10) == 3628800);



  fclose(arquivoSbas);
  printf(VERDE "Teste de fatorial passou!\n" RESETAR_COR);
}

static void roda_teste_dead_code() {
  FILE* arquivoSbas;
  unsigned char codigo[500];
  funcp funcaoSBas;
  int resultado;

  arquivoSbas = fopen("test_files/dead_code.sbas", "r");
  if (!arquivoSbas) {
    printf("Falha ao abrir arquivo SBas para leitura!\n");
    return;
  }

  printf("Testando dead code. Arquivo: dead_code.sbas\n");

  funcaoSBas = peqcomp(arquivoSbas, codigo);

  assert(funcaoSBas != NULL);

  resultado = funcaoSBas(0);
  assert(resultado == -775);
  assert(resultado != 6);

  fclose(arquivoSbas);
  printf(VERDE "Teste de dead code passou!\n" RESETAR_COR);
}

static void roda_teste_multiplos_branches() {
  FILE* arquivoSbas;
  unsigned char codigo[500];
  funcp funcaoSBas;
  int resultado;

  arquivoSbas = fopen("test_files/multiplos_branches.sbas", "r");
  if (!arquivoSbas) {
    printf("Falha ao abrir arquivo SBas para leitura!\n");
    return;
  }

  printf("Testando múltiplos branches de execução (ifs) . Arquivo: multiplos_branches.sbas\n");

  funcaoSBas = peqcomp(arquivoSbas, codigo);

  assert(funcaoSBas != NULL);

  resultado = funcaoSBas(0);
  assert(resultado == 2);

  resultado = funcaoSBas(-1);
  assert(resultado == 2);

  resultado = funcaoSBas(INT_MIN);
  assert(resultado == 2);


  resultado = funcaoSBas(1);
  assert(resultado == 3);

  resultado = funcaoSBas(INT_MAX);
  assert(resultado == 1);


  fclose(arquivoSbas);
  printf(VERDE "Teste de múltiplos branches de execução (ifs) passou!\n" RESETAR_COR);
}

static void roda_teste_multiplos_ifs_encadeados() {
  FILE* arquivoSbas;
  unsigned char codigo[500];
  funcp funcaoSBas;
  int resultado;

  arquivoSbas = fopen("test_files/multiplos_ifs_encadeados.sbas", "r");
  if (!arquivoSbas) {
    printf("Falha ao abrir arquivo SBas para leitura!\n");
    return;
  }

  printf("Testando múltiplos ifs encadeados. Arquivo: multiplos_ifs_encadeados.sbas\n");

  funcaoSBas = peqcomp(arquivoSbas, codigo);
  assert(funcaoSBas != NULL);

  resultado = funcaoSBas(-2);
  assert(resultado == 42);

  resultado = funcaoSBas(0);
  assert(resultado == 42);


  resultado = funcaoSBas(-1);
  assert(resultado == 42);

  fclose(arquivoSbas);
  printf(VERDE "Teste de múltiplos ifs encadeados passou!\n" RESETAR_COR);
}

static void roda_teste_dois_argumentos() {
  FILE* arquivoSbas;
  unsigned char codigo[500];
  funcp funcaoSBas;
  int resultado;

  arquivoSbas = fopen("test_files/dois_argumentos.sbas", "r");
  if (!arquivoSbas) {
    printf("Falha ao abrir arquivo SBas para leitura!\n");
    return;
  }

  printf("Testando dois argumentos com desvios condicionais. Arquivo: dois_argumentos.sbas\n");

  funcaoSBas = peqcomp(arquivoSbas, codigo);
  assert(funcaoSBas != NULL);

  resultado = funcaoSBas(-4, 5);
  assert(resultado == -4);

  resultado = funcaoSBas(4, 5);
  assert(resultado == 68894720);

  resultado = funcaoSBas(-1, -7);
  assert(resultado == -875);

  fclose(arquivoSbas);
  printf(VERDE "Teste de dois argumentos com iflez passou!\n" RESETAR_COR);
}

static void roda_teste_tres_argumentos() {
  FILE* arquivoSbas;
  unsigned char codigo[500];
  funcp funcaoSBas;
  int resultado;

  arquivoSbas = fopen("test_files/tres_argumentos.sbas", "r");
  if (!arquivoSbas) {
    printf("Falha ao abrir arquivo SBas para leitura!\n");
    return;
  }

  printf("Testando três argumentos com desvios condicionais. Arquivo: tres_argumentos.sbas\n");

  funcaoSBas = peqcomp(arquivoSbas, codigo);
  assert(funcaoSBas != NULL);

  resultado = funcaoSBas(1, 0, -2000);
  assert(resultado == 256);

  resultado = funcaoSBas(-1, 0, -2000);
  assert(resultado == -444);

  fclose(arquivoSbas);
  printf(VERDE "Teste de três argumentos com iflez passou!\n" RESETAR_COR);
}

static void roda_teste_multiplica_param_por_dez() {
  FILE* arquivoSbas;
  unsigned char codigo[500];
  funcp funcaoSBas;
  int resultado;

  arquivoSbas = fopen("test_files/multiplica_param_por_10.sbas", "r");
  if (!arquivoSbas) {
    printf("Falha ao abrir arquivo SBas para leitura!\n");
    return;
  }

  printf("Testando multiplicar argumento por 10. Arquivo: multiplica_param_por_10.sbas\n");

  funcaoSBas = peqcomp(arquivoSbas, codigo);
  assert(funcaoSBas != NULL);

  resultado = funcaoSBas(0);
  assert(resultado == 0);

  resultado = funcaoSBas(1);
  assert(resultado == 10);

  resultado = funcaoSBas(-1);
  assert(resultado == -10);

  fclose(arquivoSbas);
  printf(VERDE "Teste de multiplicar argumento por 10 passou!\n" RESETAR_COR);
}

static void roda_teste_multiplica() {
  FILE* arquivoSbas;
  unsigned char codigo[500];
  funcp funcaoSBas;
  int resultado;

  arquivoSbas = fopen("test_files/multiplicacao.sbas", "r");
  if (!arquivoSbas) {
    printf("Falha ao abrir arquivo SBas para leitura!\n");
    return;
  }

  printf("Testando multiplicação. Arquivo: multiplicacao.sbas\n");

  funcaoSBas = peqcomp(arquivoSbas, codigo);
  assert(funcaoSBas != NULL);

  resultado = funcaoSBas(1, 1, 1);
  assert(resultado == -100);

  resultado = funcaoSBas(0, 1, 1);
  assert(resultado == 0);

  resultado = funcaoSBas(5, -5, 1);
  assert(resultado == 2500);

  resultado = funcaoSBas(-5, -5, 1);
  assert(resultado == -2500);

  resultado = funcaoSBas(-5, -5);
  assert(resultado == -2500);

  fclose(arquivoSbas);
  printf(VERDE "Teste de multiplicação passou!\n" RESETAR_COR);
}

static void roda_teste_subtracao_1()  {
  FILE* arquivoSbas;
  unsigned char codigo[500];
  funcp funcaoSBas;
  int resultado;

  arquivoSbas = fopen("test_files/subtracao_1.sbas", "r");
  if (!arquivoSbas) {
    printf("Falha ao abrir arquivo SBas para leitura!\n");
    return;
  }

  printf("Testando subtração 1. Arquivo: subtracao_1.sbas\n");

  funcaoSBas = peqcomp(arquivoSbas, codigo);
  assert(funcaoSBas != NULL);

  resultado = funcaoSBas(1);
  assert(resultado == 0);

  fclose(arquivoSbas);
  printf(VERDE "Teste de subtração 1 passou!\n" RESETAR_COR);
}

static void roda_teste_subtracao_2()  {
  FILE* arquivoSbas;
  unsigned char codigo[500];
  funcp funcaoSBas;
  int resultado;

  arquivoSbas = fopen("test_files/subtracao_2.sbas", "r");
  if (!arquivoSbas) {
    printf("Falha ao abrir arquivo SBas para leitura!\n");
    return;
  }

  printf("Testando subtração 2. Arquivo: subtracao_2.sbas\n");

  funcaoSBas = peqcomp(arquivoSbas, codigo);
  assert(funcaoSBas != NULL);

  resultado = funcaoSBas(99, 67);
  assert(resultado == 0);

  fclose(arquivoSbas);
  printf(VERDE "Teste de subtração 2 passou!\n" RESETAR_COR);
}
