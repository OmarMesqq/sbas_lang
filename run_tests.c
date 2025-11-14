#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <limits.h>

#include "sbas.h"

#define GREEN "\033[0;32m"
#define RED "\033[31m"
#define RESET_COLOR "\033[0m"

static void run_test_parse_full_grammar();
static void run_test_callee_saveds();
static void run_test(const char* filePath, const char* testName, int paramCount, int* p1, int* p2, int* p3, int expected);
static void run_failing_test(const char* filePath, const char* testName, int paramCount, int* p1, int* p2, int* p3);

int main(void) {
  int arg1, arg2, arg3;

  run_test_parse_full_grammar();
  run_test_callee_saveds();

  run_test("test_files/return_constant.sbas", "return constant literal", 0, NULL, NULL, NULL, 16909060);
  
  arg1 = -1253512;
  run_test("test_files/return_param.sbas", "return parameter", 1, &arg1, NULL, NULL, -1253512);
  
  run_test("test_files/return_variable.sbas", "return variable", 0, NULL, NULL, NULL, 5);

  run_test("test_files/assign_constant.sbas", "constant attributions", 0, NULL, NULL, NULL, -1004);

  arg1 = 255;
  arg2 = 0;
  arg3 = 327;
  run_test("test_files/assign_parameters.sbas", "parameter attributions", 3, &arg1, &arg2, &arg3, 255);

  run_test("test_files/all_arithmetic_cases.sbas", "all arithmetic operations", 0, NULL, NULL, NULL, -746);
  

  /**
   * f(x) = x + 1 tests
   */
  arg1 = 0;
  run_test("test_files/add_one_to_arg.sbas", "f(x) = x + 1", 1, &arg1, NULL, NULL, arg1 + 1);
  arg1 = -1;
  run_test("test_files/add_one_to_arg.sbas", "f(x) = x + 1", 1, &arg1, NULL, NULL, arg1 + 1);
  arg1 = 1;
  run_test("test_files/add_one_to_arg.sbas", "f(x) = x + 1", 1, &arg1, NULL, NULL, arg1 + 1);
  arg1 = INT_MAX;
  run_test("test_files/add_one_to_arg.sbas", "f(x) = x + 1", 1, &arg1, NULL, NULL, INT_MIN);

  arg1 = 3;
  run_test("test_files/arithmetic_operation.sbas", "some arithmetic operations", 1, &arg1, NULL, NULL, 2520);
  
  /**
   * a^2 - b^2 tests
   */
  arg1 = 7;
  arg2 = 7;
  run_test("test_files/difference_of_squares.sbas", "difference of squares", 2, &arg1, &arg2, NULL, 0);
  arg1 = 10;
  arg2 = 1;
  run_test("test_files/difference_of_squares.sbas", "difference of squares", 2, &arg1, &arg2, NULL, 99);
  arg1 = 0;
  arg2 = 3;
  run_test("test_files/difference_of_squares.sbas", "difference of squares", 2, &arg1, &arg2, NULL, -9);

  /**
   * Factorial tests
   */
  arg1 = 0;
  run_test("test_files/factorial.sbas", "factorial", 1, &arg1, NULL, NULL, 1);
  arg1 = 1;
  run_test("test_files/factorial.sbas", "factorial", 1, &arg1, NULL, NULL, 1);
  arg1 = 10;
  run_test("test_files/factorial.sbas", "factorial", 1, &arg1, NULL, NULL, 3628800);

  /**
   * x + 1 > 0 tests
   */
  arg1 = 0;
  run_test("test_files/is_negative.sbas", "x + 1 > 0", 1, &arg1, NULL, NULL, 0);
  arg1 = -1;
  run_test("test_files/is_negative.sbas", "x + 1 > 0", 1, &arg1, NULL, NULL, 1);
  arg1 = -2;
  run_test("test_files/is_negative.sbas", "x + 1 > 0", 1, &arg1, NULL, NULL, 1);
  arg1 = 1;
  run_test("test_files/is_negative.sbas", "x + 1 > 0", 1, &arg1, NULL, NULL, 0);
  
  arg1 = 1;
  run_test("test_files/dead_code.sbas", "Dead code", 1, &arg1, NULL, NULL, -775);

  /**
   * Multiple branches tests
   */
  arg1 = 0;
  run_test("test_files/multiple_branches.sbas", "Multiple branches", 1, &arg1, NULL, NULL, 2);
  arg1 = -1;
  run_test("test_files/multiple_branches.sbas", "Multiple branches", 1, &arg1, NULL, NULL, 2);
  arg1 = INT_MIN;
  run_test("test_files/multiple_branches.sbas", "Multiple branches", 1, &arg1, NULL, NULL, 2);
  arg1 = 1;
  run_test("test_files/multiple_branches.sbas", "Multiple branches", 1, &arg1, NULL, NULL, 3);
  arg1 = INT_MAX;
  run_test("test_files/multiple_branches.sbas", "Multiple branches", 1, &arg1, NULL, NULL, 1);

  /**
   * Chained ifs tests
   */
  arg1 = -2;
  run_test("test_files/chained_ifs.sbas", "Chained conditionals", 1, &arg1, NULL, NULL, 42);
  arg1 = 1;
  run_test("test_files/chained_ifs.sbas", "Chained conditionals", 1, &arg1, NULL, NULL, 99);
  
  /**
   * Two parameters
   */
  arg1 = 4;
  arg2 = 5;
  run_test("test_files/two_arguments.sbas", "2 parameters", 2, &arg1, &arg2, NULL, 68894720);
  

  /**
   * Three parameters
   */
  arg1 = 1;
  arg2 = 0;
  arg3 = -2000;
  run_test("test_files/three_arguments.sbas", "3 parameters", 3, &arg1, &arg2, &arg3, 256);
  arg1 = -1;
  run_test("test_files/three_arguments.sbas", "3 parameters", 3, &arg1, &arg2, &arg3, -444);


  /**
   * Multiply param by 10
   */
  arg1 = -1;
  run_test("test_files/multiply_param_by_10.sbas", "Multiply param by 10", 1, &arg1, NULL, NULL, -10);
  
  /**
   * Three parameter multiplication
   */
  arg1 = 1;
  arg2 = 1;
  arg3 = 1;
  run_test("test_files/multiplication.sbas", "3 parameter multiplication", 3, &arg1, &arg2, &arg3, -100);

  arg1 = 1;
  run_test("test_files/subtraction_1.sbas", "Subtraction 1", 1, &arg1, NULL, NULL, 0);

  arg1 = 99;
  arg2 = 67;
  run_test("test_files/subtraction_2.sbas", "Subtraction 2", 2, &arg1, &arg2, NULL, 0);

  printf("Testing wrong syntax files...\n");
  run_failing_test("test_files/incorrect/wrong_return.sbas", "Bad return", 0, NULL, NULL, NULL);
  run_failing_test("test_files/incorrect/empty.sbas", "Empty file", 0, NULL, NULL, NULL);

  printf(GREEN "All tests passed!\n" RESET_COLOR);
  return 0;
}

/**
 * Compiles an SBas file containing all grammar. Expects to be successfully parsed.
 * The logic in this file doesn't make sense really, the only assertion is whether
 * the compiler can correctly parse it.
 */
static void run_test_parse_full_grammar() {
  FILE* sbasFile;
  funcp sbasFunction;

  sbasFile = fopen("test_files/everything.sbas", "r");
  if (!sbasFile) {
    fprintf(stderr, RED "Could not open .sbas file!\n" RESET_COLOR);
    return;
  }
  printf("Testing if grammar is correctly parsed (everything.sbas).\n");

  sbasFunction = sbasCompile(sbasFile);

  assert(sbasFunction != NULL);

  fclose(sbasFile);
  sbasCleanup(sbasFunction);
}

/**
 * Compiles an SBas file that loads values to callee-saved registers.
 * It is expected that these values are restored at the function epilogue.
 */
static void run_test_callee_saveds() {
  FILE* sbasFile;
  funcp sbasFunction;

  sbasFile = fopen("test_files/assign_variables.sbas", "r");
  if (!sbasFile) {
    fprintf(stderr, RED "Could not open .sbas file!\n" RESET_COLOR);
    return;
  }
  printf("Testing if callee-saved registers are preserved (assign_variables.sbas)...\n");

  sbasFunction = sbasCompile(sbasFile);
  assert(sbasFunction != NULL);

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

  
  (*sbasFunction)();

  // After function is called, load the register values in variables
  unsigned long rbx_after, r12_after, r13_after, r14_after, r15_after;
  asm volatile(
      "mov %%rbx, %[out_b]\n\t"
      "mov %%r12, %[out_r12]\n\t"
      "mov %%r13, %[out_r13]\n\t"
      "mov %%r14, %[out_r14]\n\t"
      "mov %%r15, %[out_r15]\n\t"
      : [out_b] "=r"(rbx_after), [out_r12] "=r"(r12_after), [out_r13] "=r"(r13_after), [out_r14] "=r"(r14_after), [out_r15] "=r"(r15_after));

  assert(rbx_after == rbx && "rbx was altered and not restored!");
  assert(r12_after == r12 && "r12 was altered and not restored!");
  assert(r13_after == r13 && "r13 was altered and not restored!");
  assert(r14_after == r14 && "r14 was altered and not restored!");
  assert(r15_after == r15 && "r15 was altered and not restored!");

  fclose(sbasFile);
  sbasCleanup(sbasFunction);
}

/**
 * Compiles an `.sbas` file and asserts its return result
 * @param filePath relative or absoulute path to the `.sbas` file
 * @param testName name of the test echoed in `stdout`
 * @param paramCount amount of parameters that the SBas function takes [0, 3]
 * @param p1 pointer to the first parameter of SBas function
 * @param p2 pointer to the second parameter of SBas function
 * @param p3 pointer to the third parameter of SBas function
 * @param expected value to assert on SBas function return
 */
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
  funcp sbasFunction;
  int res;

  sbasFile = fopen(filePath, "r");
  if (!sbasFile) {
    fprintf(stderr, RED "Could not open sbas file: %s.\n" RESET_COLOR, filePath);
    return;
  }

  sbasFunction = sbasCompile(sbasFile);
  assert(sbasFunction != NULL);

  switch (paramCount) {
    case 0:
      printf("Running test %s for file %s with no params\n", testName, filePath);
      res = (*sbasFunction)();
      break;
    case 1:
      printf("Running test %s for file %s with one param: p1 = %d\n", testName, filePath, *p1);
      res = sbasFunction(*p1);
      break;
    case 2:
      printf("Running test %s for file %s with two params: p1 = %d, p2 = %d\n", testName, filePath, *p1, *p2);
      res = sbasFunction(*p1, *p2);
      break;
    case 3:
      printf("Running test %s for file %s with three params: p1 = %d, p2 = %d, p3 = %d\n", testName, filePath, *p1, *p2, *p3);
      res = sbasFunction(*p1, *p2, *p3);
      break;
    default:
      fprintf(stderr, RED "run_tests: invalid paramCount: %d" RESET_COLOR, paramCount);
      break;
  }

  fclose(sbasFile);
  sbasCleanup(sbasFunction);
  if (res != expected) {
    fprintf(stderr, RED "Test %s FAILED! Expected: %d, got: %d\n" RESET_COLOR, testName, expected, res);
    exit(-1);
  }
}

/**
 * Attempts to compile an incorrect `.sbas` file and asserts compilation failure
 * @param filePath relative or absoulute path to the `.sbas` file
 * @param testName name of the test echoed in `stdout`
 * @param paramCount amount of parameters that the SBas function takes [0, 3]
 * @param p1 pointer to the first parameter of SBas function
 * @param p2 pointer to the second parameter of SBas function
 * @param p3 pointer to the third parameter of SBas function
 */
static void run_failing_test(const char* filePath, const char* testName, int paramCount, int* p1, int* p2, int* p3) {
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
  funcp sbasFunction;

  sbasFile = fopen(filePath, "r");
  if (!sbasFile) {
    fprintf(stderr, RED "Could not open sbas file: %s.\n" RESET_COLOR, filePath);
    return;
  }

  sbasFunction = sbasCompile(sbasFile);
  assert(sbasFunction == NULL);

  fclose(sbasFile);
  sbasCleanup(sbasFunction);
}