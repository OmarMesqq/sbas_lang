#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <limits.h>

#include "sbas.h"

#define GREEN "\033[0;32m"
#define RESET_COLOR "\033[0m"

static void roda_teste_parseia_toda_a_gramatica();
static void roda_teste_retorno_zero();
static void roda_teste_retorno_constante();
static void roda_teste_retorno_variavel();
static void roda_teste_overflow();
static void roda_teste_atribuicao_de_constante();
static void roda_teste_atribuicao_de_parametro();
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
static void run_test(const char* filePath, const char* testName, int paramCount, int* p1, int* p2, int* p3, int expected);

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

  arquivoSbas = fopen("test_files/everything.sbas", "r");
  if (!arquivoSbas) {
    printf("Falha ao abrir arquivo SBas para leitura!\n");
    return;
  }
  printf("Testing if grammar is correctly parsed.\n");

  // Gera a função
  funcaoSBas = sbasCompile(arquivoSbas, codigo);

  // O ponteiro para esta não deve ser NULL
  assert(funcaoSBas != NULL);

  fclose(arquivoSbas);
  printf(GREEN "Full grammar parse test passed!\n" RESET_COLOR);
}

static void roda_teste_salva_callee_saveds() {
  FILE* arquivoSbas;
  unsigned char codigo[500];
  funcp funcaoSBas;

  arquivoSbas = fopen("test_files/assign_variables.sbas", "r");
  if (!arquivoSbas) {
    printf("Falha ao abrir arquivo SBas para leitura!\n");
    return;
  }
  printf("Testing if callee-saved registers are preserved...\n");

  funcaoSBas = sbasCompile(arquivoSbas, codigo);
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

  assert(rbx_depois == rbx && "rbx was altered and not restored!");
  assert(r12_depois == r12 && "r12 was altered and not restored!");
  assert(r13_depois == r13 && "r13 was altered and not restored!");
  assert(r14_depois == r14 && "r14 was altered and not restored!");
  assert(r15_depois == r15 && "r15 was altered and not restored!");

  fclose(arquivoSbas);
  printf(GREEN "Callee-saved registers test passed!\n" RESET_COLOR);
}

static void roda_teste_retorno_zero() {
  FILE* arquivoSbas;
  unsigned char codigo[500];
  funcp funcaoSBas;
  int resultado;

  arquivoSbas = fopen("test_files/return_zero.sbas", "r");
  if (!arquivoSbas) {
    printf("Falha ao abrir arquivo SBas para leitura!\n");
    return;
  }

  // Gera a função
  funcaoSBas = sbasCompile(arquivoSbas, codigo);

  // O ponteiro para esta não deve ser NULL
  assert(funcaoSBas != NULL);

  // Obtem o resultado
  resultado = (*funcaoSBas)();

  // Garante resultado esperado
  assert(resultado == 0);

  fclose(arquivoSbas);
  printf(GREEN "Teste de retorno zero passou!\n" RESET_COLOR);
}

static void roda_teste_retorno_constante() {
  FILE* arquivoSbas;
  unsigned char codigo[500];
  funcp funcaoSBas;
  int resultado;

  arquivoSbas = fopen("test_files/return_constant.sbas", "r");
  if (!arquivoSbas) {
    printf("Falha ao abrir arquivo SBas para leitura!\n");
    return;
  }

  // Gera a função
  funcaoSBas = sbasCompile(arquivoSbas, codigo);

  // O ponteiro para esta não deve ser NULL
  assert(funcaoSBas != NULL);

  // Obtem o resultado
  resultado = (*funcaoSBas)();

  // Garante resultado esperado
  assert(resultado == 16909060);

  fclose(arquivoSbas);
  printf(GREEN "Teste de retorno de uma constante passou!\n" RESET_COLOR);
}

static void roda_teste_retorno_de_um_param() {
  FILE* arquivoSbas;
  unsigned char codigo[500];
  funcp funcaoSBas;
  int resultado;

  arquivoSbas = fopen("test_files/return_param.sbas", "r");
  if (!arquivoSbas) {
    printf("Falha ao abrir arquivo SBas para leitura!\n");
    return;
  }

  // Gera a função
  funcaoSBas = sbasCompile(arquivoSbas, codigo);

  // O ponteiro para esta não deve ser NULL
  assert(funcaoSBas != NULL);

  resultado = (*funcaoSBas)(16909060);  
  assert(resultado == 16909060);

  resultado = (*funcaoSBas)(-1253512);  
  assert(resultado == -1253512);

  fclose(arquivoSbas);
  printf(GREEN "Teste de retorno de um param passou!\n" RESET_COLOR);
}

static void roda_teste_retorno_variavel() {
  FILE* arquivoSbas;
  unsigned char codigo[500];
  funcp funcaoSBas;
  int resultado;

  arquivoSbas = fopen("test_files/return_variable.sbas", "r");
  if (!arquivoSbas) {
    printf("Falha ao abrir arquivo SBas para leitura!\n");
    return;
  }

  // Gera a função
  funcaoSBas = sbasCompile(arquivoSbas, codigo);

  // O ponteiro para esta não deve ser NULL
  assert(funcaoSBas != NULL);

  // Obtem o resultado
  resultado = (*funcaoSBas)();

  // Garante resultado esperado
  assert(resultado == 5);

  fclose(arquivoSbas);
  printf(GREEN "Teste de retorno de variável local passou!\n" RESET_COLOR);
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
  funcaoSBas = sbasCompile(arquivoSbas, codigo);

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
  funcaoSBas = sbasCompile(arquivoSbas, codigo);

  // O ponteiro para esta não deve ser NULL
  assert(funcaoSBas != NULL);

  // Obtem o resultado
  resultado = (*funcaoSBas)();

  // Espera que INT_MIN - 1 = INT_MAX
  assert(resultado == 2147483647);

  fclose(arquivoSbas);
  printf(GREEN "Teste de overflow passou!\n" RESET_COLOR);
}

static void roda_teste_atribuicao_de_constante() {
  FILE* arquivoSbas;
  unsigned char codigo[500];
  funcp funcaoSBas;
  int resultado;

  arquivoSbas = fopen("test_files/assign_constant.sbas", "r");
  if (!arquivoSbas) {
    printf("Falha ao abrir arquivo SBas para leitura!\n");
    return;
  }

  // Gera a função
  funcaoSBas = sbasCompile(arquivoSbas, codigo);

  // O ponteiro para esta não deve ser NULL
  assert(funcaoSBas != NULL);

  resultado = (*funcaoSBas)();
  assert(resultado == -1004);

  fclose(arquivoSbas);
  printf(GREEN "Teste de atribuição de constantes passou!\n" RESET_COLOR);
}

static void roda_teste_atribuicao_de_parametro() {
  FILE* arquivoSbas;
  unsigned char codigo[500];
  funcp funcaoSBas;

  arquivoSbas = fopen("test_files/assign_parameters.sbas", "r");
  if (!arquivoSbas) {
    printf("Falha ao abrir arquivo SBas para leitura!\n");
    return;
  }

  // Gera a função
  funcaoSBas = sbasCompile(arquivoSbas, codigo);

  // O ponteiro para esta não deve ser NULL
  assert(funcaoSBas != NULL);

  fclose(arquivoSbas);
  printf(GREEN "Teste de atribuição de parâmetros passou!\n" RESET_COLOR);
}


static void roda_teste_todos_casos_operacao_aritmetica() {
  FILE* arquivoSbas;
  unsigned char codigo[500];
  funcp funcaoSBas;
  int resultado;

  arquivoSbas = fopen("test_files/all_arithmetic_cases.sbas", "r");
  if (!arquivoSbas) {
    printf("Falha ao abrir arquivo SBas para leitura!\n");
    return;
  }

  // Gera a função
  funcaoSBas = sbasCompile(arquivoSbas, codigo);

  // O ponteiro para esta não deve ser NULL
  assert(funcaoSBas != NULL);

  resultado = funcaoSBas();
  assert(resultado == -746);

  fclose(arquivoSbas);
  printf(GREEN "Teste de todos os casos de operações aritméticas passou!\n" RESET_COLOR);
}

static void roda_teste_operacao_aritmetica() {
  FILE* arquivoSbas;
  unsigned char codigo[500];
  funcp funcaoSBas;
  int resultado;

  arquivoSbas = fopen("test_files/arithmetic_operation.sbas", "r");
  if (!arquivoSbas) {
    printf("Falha ao abrir arquivo SBas para leitura!\n");
    return;
  }

  // Gera a função
  funcaoSBas = sbasCompile(arquivoSbas, codigo);

  // O ponteiro para esta não deve ser NULL
  assert(funcaoSBas != NULL);

  resultado = (*funcaoSBas)(3);

  assert(resultado == 2520);

  fclose(arquivoSbas);
  printf(GREEN "Teste de operações aritméticas passou!\n" RESET_COLOR);
}

static void roda_teste_soma_um_ao_argumento() {
  FILE* arquivoSbas;
  unsigned char codigo[500];
  funcp funcaoSBas;
  int resultado;

  arquivoSbas = fopen("test_files/add_one_to_arg.sbas", "r");
  if (!arquivoSbas) {
    printf("Falha ao abrir arquivo SBas para leitura!\n");
    return;
  }

  // Gera a função
  funcaoSBas = sbasCompile(arquivoSbas, codigo);

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
  printf(GREEN "Teste de somar 1 ao primeiro argumento passou!\n" RESET_COLOR);
}

static void roda_teste_diferenca_de_quadrados() {
  FILE* arquivoSbas;
  unsigned char codigo[500];
  funcp funcaoSBas;

  arquivoSbas = fopen("test_files/difference_of_squares.sbas", "r");
  if (!arquivoSbas) {
    printf("Falha ao abrir arquivo SBas para leitura!\n");
    return;
  }

  funcaoSBas = sbasCompile(arquivoSbas, codigo);

  assert(funcaoSBas != NULL);

  assert(funcaoSBas(4, 2) == 12);   // 16 - 4 = 12
  assert(funcaoSBas(5, 0) == 25);   // 25 - 0 = 25
  assert(funcaoSBas(7, 7) == 0);    // 49 - 49 = 0
  assert(funcaoSBas(10, 1) == 99);  // 100 - 1 = 99
  assert(funcaoSBas(-3, 2) == 5);   // 9 - 4 = 5
  assert(funcaoSBas(0, 3) == -9);   // 0 - 9 = -9
  assert(funcaoSBas(-5, -4) == 9);  // 25 - 16 = 9

  fclose(arquivoSbas);
  printf(GREEN "Teste de diferença de quadrados passou!\n" RESET_COLOR);
}

static void roda_teste_eh_negativo() {
  FILE* arquivoSbas;
  unsigned char codigo[500];
  funcp funcaoSBas;
  int resultado;

  arquivoSbas = fopen("test_files/is_negative.sbas", "r");
  if (!arquivoSbas) {
    printf("Falha ao abrir arquivo SBas para leitura!\n");
    return;
  }

  funcaoSBas = sbasCompile(arquivoSbas, codigo);

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
  printf(GREEN "Teste de se número somado a um é negativo passou!\n" RESET_COLOR);
}

static void roda_teste_fatorial() {
  FILE* arquivoSbas;
  unsigned char codigo[500];
  funcp funcaoSBas;
  int resultado;

  arquivoSbas = fopen("test_files/factorial.sbas", "r");
  if (!arquivoSbas) {
    printf("Falha ao abrir arquivo SBas para leitura!\n");
    return;
  }

  funcaoSBas = sbasCompile(arquivoSbas, codigo);

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
  printf(GREEN "Teste de fatorial passou!\n" RESET_COLOR);
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

  funcaoSBas = sbasCompile(arquivoSbas, codigo);

  assert(funcaoSBas != NULL);

  resultado = funcaoSBas(0);
  assert(resultado == -775);
  assert(resultado != 6);

  fclose(arquivoSbas);
  printf(GREEN "Teste de dead code passou!\n" RESET_COLOR);
}

static void roda_teste_multiplos_branches() {
  FILE* arquivoSbas;
  unsigned char codigo[500];
  funcp funcaoSBas;
  int resultado;

  arquivoSbas = fopen("test_files/multiple_branches.sbas", "r");
  if (!arquivoSbas) {
    printf("Falha ao abrir arquivo SBas para leitura!\n");
    return;
  }

  funcaoSBas = sbasCompile(arquivoSbas, codigo);

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
  printf(GREEN "Teste de múltiplos branches de execução (ifs) passou!\n" RESET_COLOR);
}

static void roda_teste_multiplos_ifs_encadeados() {
  FILE* arquivoSbas;
  unsigned char codigo[500];
  funcp funcaoSBas;
  int resultado;

  arquivoSbas = fopen("test_files/chained_ifs.sbas", "r");
  if (!arquivoSbas) {
    printf("Falha ao abrir arquivo SBas para leitura!\n");
    return;
  }

  funcaoSBas = sbasCompile(arquivoSbas, codigo);
  assert(funcaoSBas != NULL);

  resultado = funcaoSBas(-2);
  assert(resultado == 42);

  resultado = funcaoSBas(0);
  assert(resultado == 42);


  resultado = funcaoSBas(-1);
  assert(resultado == 42);

  fclose(arquivoSbas);
  printf(GREEN "Teste de múltiplos ifs encadeados passou!\n" RESET_COLOR);
}

static void roda_teste_dois_argumentos() {
  FILE* arquivoSbas;
  unsigned char codigo[500];
  funcp funcaoSBas;
  int resultado;

  arquivoSbas = fopen("test_files/two_arguments.sbas", "r");
  if (!arquivoSbas) {
    printf("Falha ao abrir arquivo SBas para leitura!\n");
    return;
  }

  funcaoSBas = sbasCompile(arquivoSbas, codigo);
  assert(funcaoSBas != NULL);

  resultado = funcaoSBas(-4, 5);
  assert(resultado == -4);

  resultado = funcaoSBas(4, 5);
  assert(resultado == 68894720);

  resultado = funcaoSBas(-1, -7);
  assert(resultado == -875);

  fclose(arquivoSbas);
  printf(GREEN "Teste de dois argumentos com iflez passou!\n" RESET_COLOR);
}

static void roda_teste_tres_argumentos() {
  FILE* arquivoSbas;
  unsigned char codigo[500];
  funcp funcaoSBas;
  int resultado;

  arquivoSbas = fopen("test_files/three_arguments.sbas", "r");
  if (!arquivoSbas) {
    printf("Falha ao abrir arquivo SBas para leitura!\n");
    return;
  }

  funcaoSBas = sbasCompile(arquivoSbas, codigo);
  assert(funcaoSBas != NULL);

  resultado = funcaoSBas(1, 0, -2000);
  assert(resultado == 256);

  resultado = funcaoSBas(-1, 0, -2000);
  assert(resultado == -444);

  fclose(arquivoSbas);
  printf(GREEN "Teste de três argumentos com iflez passou!\n" RESET_COLOR);
}

static void roda_teste_multiplica_param_por_dez() {
  FILE* arquivoSbas;
  unsigned char codigo[500];
  funcp funcaoSBas;
  int resultado;

  arquivoSbas = fopen("test_files/multiply_param_by_10.sbas", "r");
  if (!arquivoSbas) {
    printf("Falha ao abrir arquivo SBas para leitura!\n");
    return;
  }

  funcaoSBas = sbasCompile(arquivoSbas, codigo);
  assert(funcaoSBas != NULL);

  resultado = funcaoSBas(0);
  assert(resultado == 0);

  resultado = funcaoSBas(1);
  assert(resultado == 10);

  resultado = funcaoSBas(-1);
  assert(resultado == -10);

  fclose(arquivoSbas);
  printf(GREEN "Teste de multiplicar argumento por 10 passou!\n" RESET_COLOR);
}

static void roda_teste_multiplica() {
  FILE* arquivoSbas;
  unsigned char codigo[500];
  funcp funcaoSBas;
  int resultado;

  arquivoSbas = fopen("test_files/multiplication.sbas", "r");
  if (!arquivoSbas) {
    printf("Falha ao abrir arquivo SBas para leitura!\n");
    return;
  }

  funcaoSBas = sbasCompile(arquivoSbas, codigo);
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
  printf(GREEN "Teste de multiplicação passou!\n" RESET_COLOR);
}

static void roda_teste_subtracao_1()  {
  FILE* arquivoSbas;
  unsigned char codigo[500];
  funcp funcaoSBas;
  int resultado;

  arquivoSbas = fopen("test_files/subtraction_1.sbas", "r");
  if (!arquivoSbas) {
    printf("Falha ao abrir arquivo SBas para leitura!\n");
    return;
  }

  funcaoSBas = sbasCompile(arquivoSbas, codigo);
  assert(funcaoSBas != NULL);

  resultado = funcaoSBas(1);
  assert(resultado == 0);

  fclose(arquivoSbas);
  printf(GREEN "Teste de subtração 1 passou!\n" RESET_COLOR);
}

static void roda_teste_subtracao_2()  {
  FILE* arquivoSbas;
  unsigned char codigo[500];
  funcp funcaoSBas;
  int resultado;

  arquivoSbas = fopen("test_files/subtraction_2.sbas", "r");
  if (!arquivoSbas) {
    printf("Falha ao abrir arquivo SBas para leitura!\n");
    return;
  }

  funcaoSBas = sbasCompile(arquivoSbas, codigo);
  assert(funcaoSBas != NULL);

  resultado = funcaoSBas(99, 67);
  assert(resultado == 0);

  fclose(arquivoSbas);
  printf(GREEN "Teste de subtração 2 passou!\n" RESET_COLOR);
}

static void run_test(const char* filePath, const char* testName, int paramCount, int* p1, int* p2, int* p3, int expected) {
  if (!filePath || !testName) {
    fprintf(stderr, "run_test: filePath or testName were not passed.\n");
    return;
  }

  if (paramCount < 0 || paramCount > 3) {
    fprintf(stderr, "run_test: SBas functions have between 0 and 3 arguments.\n");
    return;
  }

  if ((paramCount == 1) && p1 == NULL) {
    return;
  } 
  if ((paramCount == 2) && ((p1 == NULL) || (p2 == NULL))) {
    return;
  } if ((paramCount == 3) && ((p1 == NULL) || (p2 == NULL) || (p3 == NULL))) {
    return;
  }


  FILE* sbasFile;
  unsigned char code[500];
  funcp sbasFunction;
  int res;

  sbasFile = fopen(filePath, "r");
  if (!sbasFile) {
    fprintf(stderr, "Could not open sbas file: %s.\n", filePath);
    return;
  }

  printf("Running test %s for file %s\n", testName, filePath);

  sbasFunction = sbasCompile(sbasFile, code);
  assert(sbasFunction != NULL);

  res = sbasFunction(99, 67);
  assert(res == expected);

  fclose(sbasFile);
  printf("%sTest %s passed for file %s!%s\n", GREEN, testName, filePath, RESET_COLOR);
}
