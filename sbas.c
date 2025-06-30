#include "sbas.h"

#include <stdio.h>
#include <stdlib.h>

#define DEBUG
#define VERMELHO "\033[31m"
#define RESETAR_COR "\033[0m"
#define MAX_LINHAS 30

/**
 * Representa uma variável ou símbolo no código-fonte SBas.
 * 
 * Campos:
 * - line: número da linha onde o símbolo foi declarado.
 * - offset: deslocamento em bytes no stack frame onde a variável está armazenada.
 */
typedef struct tabelaDeSimbolos {
  unsigned char line;
  int offset;
} SymbolTable;

/**
 * Representa uma entrada na tabela de realocação para instruções de desvio condicional.
 * 
 * Campos:
 * - lineToBeResolved: linha do código SBas que contém o desvio.
 * - offset: posição no código de máquina onde o endereço de destino deve ser preenchido.
 */
typedef struct dicionarioDeRelocacao {
  unsigned char lineToBeResolved;
  int offset;
} RelocationTable;

/**
 * Representa um registrador de propósito geral e informações de prefixo REX.
 * 
 * Campos:
 * - reg_code: código numérico do registrador (0–7).
 * - rex: flag indicando se o registrador está nos registradores estendidos (8–15).
 */
typedef struct {
  int reg_code;
  char rex;
} RegInfo;


static void error(const char* msg, int line);
static void escreve_prologo(unsigned char codigo[], int* pos);
static void inicializa_registradores(unsigned char codigo[], int* pos);
static void salva_regs_callee_saved(unsigned char codigo[], int* pos);
static void restaura_regs_callee_saved(unsigned char codigo[], int* pos);
static void escreve_epilogo(unsigned char codigo[], int* pos);
static void escreve_inteiro_em_4bytes_hex(unsigned char codigo[], int* pos, int inteiro);
static void escreve_retorno_constante(unsigned char codigo[], int* pos, int valorRetorno);
static void escreve_retorno_variavel(unsigned char codigo[], int* pos, int varIdx);
static void escreve_atribuicao(unsigned char codigo[], int* pos, int idxVar, char varpcPrefix, int idxVarpc);
static void escreve_operacao_aritmetica(unsigned char codigo[], int* pos, int idxVar, char varc1Prefix, int idxVarc1, char op, char varc2Prefix, int idxVarc2);
static void escreve_cmpl_de_desvio(unsigned char codigo[], int* pos, int varIndex);
static RegInfo get_local_var_reg(int idx);
static RegInfo get_param_reg(int idx);

funcp peqcomp(FILE* f, unsigned char codigo[]) {
  unsigned line = 1;
  char lineBuffer[256];
  int pos = 0;         // posição no buffer código
  int relocCount = 0;  // quantas linhas devem ter o offset resolvido para o jump

  SymbolTable* st = malloc((MAX_LINHAS + 1) * sizeof(SymbolTable));
  RelocationTable* rt = malloc((MAX_LINHAS + 1) * sizeof(RelocationTable));
  if (!st || !rt) {
    printf("Falha ao alocar tabela de símbolos e/ou dicionário de relocação!\n");
    return NULL;
  }

  escreve_prologo(codigo, &pos);
  salva_regs_callee_saved(codigo, &pos);
  inicializa_registradores(codigo, &pos);

  /**
   * Primeiro passe no código fonte: emite maior parte das instruções
   * e deixa "buracos" de 4 bytes de zero para jumps
   */
  while (fgets(lineBuffer, sizeof(lineBuffer), f) && line <= MAX_LINHAS) {
    st[line].line = line;
    st[line].offset = pos;
    switch (lineBuffer[0]) {
      case 'r': { /* retorno */
        int varc;

        // retorno de uma variável local (ex.: ret v2)
        if (sscanf(lineBuffer, "ret v%d", &varc) == 1) {
          escreve_retorno_variavel(codigo, &pos, varc);
        }
        // retorno de uma constante (ex.: ret $-10)
        else if (sscanf(lineBuffer, "ret $%d", &varc) == 1) {
          escreve_retorno_constante(codigo, &pos, varc);
        }
        // erro de sintaxe
        else {
          error("comando 'ret' inválido: esperado 'ret <var|$int>", line);
          return NULL;
        }

        break;
      }
      case 'v': { /* atribuicao e operacao aritmetica */
        int idxVar;
        char operator;
        if (sscanf(lineBuffer, "v%d %c", &idxVar, &operator) != 2) {
          error("comando inválido: esperada atribuição (vX: varpc) ou op. aritmética (vX = varc op varc)", line);
        }

        // Só são permitidas 5 variáveis locais (v1, v2, v3, v4, v5)
        if (idxVar < 1 || idxVar > 5) {
          error("índice de variável inválido: só são permitidas 5 variáveis locais!", line);
        }

        // atribuição
        if (operator == ':') {
          char varpcPrefix;
          int idxVarpc;
          if (sscanf(lineBuffer, "v%d : %c%d", &idxVar, &varpcPrefix, &idxVarpc) != 3) {
            error("comando inválido: esperada atribuição (vX: <vX|pX|$num>)", line);
          }
          escreve_atribuicao(codigo, &pos, idxVar, varpcPrefix, idxVarpc);
        }
        // operação aritmética
        else if (operator == '=') {
          int idxVar;
          char varc1Prefix;
          int idxVarc1;
          char op;
          char varc2Prefix;
          int idxVarc2;

          if (sscanf(lineBuffer, "v%d = %c%d %c %c%d", &idxVar, &varc1Prefix, &idxVarc1, &op, &varc2Prefix, &idxVarc2) != 6) {
            error("comando inválido: esperada op. aritmética (vX = <vX|$num> op <vX|$num>)", line);
          }

          if (op != '+' && op != '-' && op != '*') {
            error("comando inválido: só são permitidas adição, subtração e multiplicação em SBas", line);
          }

          escreve_operacao_aritmetica(codigo, &pos, idxVar, varc1Prefix, idxVarc1, op, varc2Prefix, idxVarc2);
        }

        break;
      }
      case 'i': { /* desvio condicional */
        int varIndex;
        unsigned lineTarget;
        if (sscanf(lineBuffer, "iflez v%d %u", &varIndex, &lineTarget) != 2) {
          error("comando invalido", line);
          return NULL;
        }

        escreve_cmpl_de_desvio(codigo, &pos, varIndex);

        // Reserva espaço para salto condicional na tabela de relocação
        rt[relocCount].lineToBeResolved = lineTarget;
        rt[relocCount].offset = pos;
        relocCount++;
        pos += 4; // 4 bytes para offset

        break;
      }
      default:
        error("comando desconhecido", line);
    }
    line++;
    fscanf(f, " ");
  }

  /**
   * Segundo passe no código fonte: preenche placeholders de saltos condicionais
   * com os devidos offsets
   */
  for (int i = 0; i < relocCount; i++) {
    int targetOffset = st[rt[i].lineToBeResolved].offset;
    int jumpFrom = rt[i].offset;

    // offset relativo à próxima instrução
    int rel32 = targetOffset - (jumpFrom + 4);
    escreve_inteiro_em_4bytes_hex(codigo, &jumpFrom, rel32);
  }
  
  free(st);
  free(rt);

  #ifdef DEBUG
  printf("peqcomp escreveu %d bytes no buffer.\n", pos);
  #endif

  return (funcp)codigo;
}

static void error(const char* msg, int line) { 
  fprintf(stderr, "%s[linha %d no arquivo .sbas]: %s%s\n", VERMELHO, line, msg, RESETAR_COR);
}

/**
 * Configura início de uma função x86-64. Salva base do RA
 * anterior e configura a base do RA atual
 */
static void escreve_prologo(unsigned char codigo[], int* pos) {
  // pushq %rbp é a primeira instrução de qualquer função
  codigo[*pos] = 0x55;
  (*pos)++;
  // seguida de movq %rsp, %rbp
  codigo[*pos] = 0x48;
  (*pos)++;
  codigo[*pos] = 0x89;
  (*pos)++;
  codigo[*pos] = 0xe5;
  (*pos)++;
}

/**
 * Apesar do projeto não especificar nada sobre variáveis não inicializadas,
 * optei por zerar todos os registradores associados antes de começar a
 * escrever as operações
 */
static void inicializa_registradores(unsigned char codigo[], int* pos) {
  // xorl %ebx,%ebx
  codigo[*pos] = 0x31;
  (*pos)++;
  codigo[*pos] = 0xdb;
  (*pos)++;

  // xorl  %r12d,%r12d
  codigo[*pos] = 0x45;
  (*pos)++;
  codigo[*pos] = 0x31;
  (*pos)++;
  codigo[*pos] = 0xe4;
  (*pos)++;


  // xorl  %r13d, %r13d
  codigo[*pos] = 0x45;
  (*pos)++;
  codigo[*pos] = 0x31;
  (*pos)++;
  codigo[*pos] = 0xed;
  (*pos)++;

  // xorl  %r14d, %r14d
  codigo[*pos] = 0x45;
  (*pos)++;
  codigo[*pos] = 0x31;
  (*pos)++;
  codigo[*pos] = 0xf6;
  (*pos)++;

  // xorl  %r15d, %r15d
  codigo[*pos] = 0x45;
  (*pos)++;
  codigo[*pos] = 0x31;
  (*pos)++;
  codigo[*pos] = 0xff;
  (*pos)++;

}

/**
 * Usei os registradores %ebx para v1 e %r12d a %r15d para v2 - v5.
 * Como são callee-saveds, devemos salvá-los
 */
static void salva_regs_callee_saved(unsigned char codigo[], int* pos) {
  // subq $48, %rsp
  codigo[*pos] = 0x48;
  (*pos)++;
  codigo[*pos] = 0x83;
  (*pos)++;
  codigo[*pos] = 0xEC;
  (*pos)++;
  codigo[*pos] = 0x30;
  (*pos)++;

  // movq %rbx, -8(%rbp)
  codigo[*pos] = 0x48;
  (*pos)++;
  codigo[*pos] = 0x89;
  (*pos)++;
  codigo[*pos] = 0x5D;
  (*pos)++;
  codigo[*pos] = 0xF8;
  (*pos)++;

  // movq %r12, -16(%rbp)
  codigo[*pos] = 0x4C;
  (*pos)++;
  codigo[*pos] = 0x89;
  (*pos)++;
  codigo[*pos] = 0x65;
  (*pos)++;
  codigo[*pos] = 0xF0;
  (*pos)++;

  // movq %r13, -24(%rbp)
  codigo[*pos] = 0x4C;
  (*pos)++;
  codigo[*pos] = 0x89;
  (*pos)++;
  codigo[*pos] = 0x6D;
  (*pos)++;
  codigo[*pos] = 0xE8;
  (*pos)++;

  // movq %r14, -32(%rbp)
  codigo[*pos] = 0x4C;
  (*pos)++;
  codigo[*pos] = 0x89;
  (*pos)++;
  codigo[*pos] = 0x75;
  (*pos)++;
  codigo[*pos] = 0xE0;
  (*pos)++;

  // movq %r15, -40(%rbp)
  codigo[*pos] = 0x4C;
  (*pos)++;
  codigo[*pos] = 0x89;
  (*pos)++;
  codigo[*pos] = 0x7D;
  (*pos)++;
  codigo[*pos] = 0xD8;
  (*pos)++;
}

/**
 * Assim que a função SBas encerrar, deve-se restaurar os
 * callee-saveds
 */
static void restaura_regs_callee_saved(unsigned char codigo[], int* pos) {
  // movq -8(%rbp), %rbx
  codigo[*pos] = 0x48;
  (*pos)++;
  codigo[*pos] = 0x8B;
  (*pos)++;
  codigo[*pos] = 0x5D;
  (*pos)++;
  codigo[*pos] = 0xF8;
  (*pos)++;

  // movq -16(%rbp), %r12
  codigo[*pos] = 0x4C;
  (*pos)++;
  codigo[*pos] = 0x8B;
  (*pos)++;
  codigo[*pos] = 0x65;
  (*pos)++;
  codigo[*pos] = 0xF0;
  (*pos)++;

  // movq -24(%rbp), %r13
  codigo[*pos] = 0x4C;
  (*pos)++;
  codigo[*pos] = 0x8B;
  (*pos)++;
  codigo[*pos] = 0x6D;
  (*pos)++;
  codigo[*pos] = 0xE8;
  (*pos)++;

  // movq -32(%rbp), %r14
  codigo[*pos] = 0x4C;
  (*pos)++;
  codigo[*pos] = 0x8B;
  (*pos)++;
  codigo[*pos] = 0x75;
  (*pos)++;
  codigo[*pos] = 0xE0;
  (*pos)++;

  // movq -40(%rbp), %r15
  codigo[*pos] = 0x4C;
  (*pos)++;
  codigo[*pos] = 0x8B;
  (*pos)++;
  codigo[*pos] = 0x7D;
  (*pos)++;
  codigo[*pos] = 0xD8;
  (*pos)++;
}

/**
 * Desfaz RA atual: leave e ret
 */
static void escreve_epilogo(unsigned char codigo[], int* pos) {
  // toda função termina com leave e ret
  codigo[*pos] = 0xc9;
  (*pos)++;

  codigo[*pos] = 0xc3;
  (*pos)++;
}

/**
 * Recebe um inteiro com sinal em base 10 e o escreve em hexadecimal, little
 * endian no buffer. Usado para valores imediatos SBas (como v1: $5) e offsets
 * de saltos
 */
static void escreve_inteiro_em_4bytes_hex(unsigned char codigo[], int* pos, int inteiro) {
  codigo[*pos] = inteiro & 0xFF;
  (*pos)++;
  codigo[*pos] = (inteiro >> 8) & 0xFF;
  (*pos)++;
  codigo[*pos] = (inteiro >> 16) & 0xFF;
  (*pos)++;
  codigo[*pos] = (inteiro >> 24) & 0xFF;
  (*pos)++;
}

/**
 * Escreve código máquina para retornar um valor imediato imm32 (colocá-lo no %eax)
 * Prossegue com a restauração de callee-saves e epílogo da função também
 */
static void escreve_retorno_constante(unsigned char codigo[], int* pos, int valorRetorno) {
  codigo[*pos] = 0xb8;  // movl ..., %eax
  (*pos)++;

  escreve_inteiro_em_4bytes_hex(codigo, pos, valorRetorno);

  restaura_regs_callee_saved(codigo, pos);
  escreve_epilogo(codigo, pos);
}

/**
 * Escreve código máquina para retornar uma variável SBas (v1 a v5)
 * Prossegue com a restauração de callee-saves e epílogo da função também
 */
static void escreve_retorno_variavel(unsigned char codigo[], int* pos, int varIdx) {
  RegInfo reg = get_local_var_reg(varIdx);
  if (reg.reg_code == -1) {
    return;
  }

  // Escreve byte REX se necessário
  if (reg.rex) {
    codigo[(*pos)++] = 0x44;  // REX com bit R setado em 1 (para reg de origem)
  }

  // Escreve byte da operação `mov`
  codigo[(*pos)++] = 0x89;

  /**
   * Constrói o byte ModRM:
   * mod | reg | r/m
   * bb  | bbb | bbb
   *
   * Os dois bits 7 e 6 (mod) estão setados em 1, indicando op. entre registradores
   * Os três bits reg correspondem a reg_code, o registrador de origem
   * Os três bits r/m indicam o registrador de destino (no caso de retorno, %eax = 000)
   */
  unsigned char modrm = 0xC0 + (reg.reg_code << 3);
  codigo[(*pos)++] = modrm;

  restaura_regs_callee_saved(codigo, pos);
  escreve_epilogo(codigo, pos);
}

/**
 * Escreve código de máquina de uma atribuição SBas
 * vX: <vX|pX|$num>
 */
static void escreve_atribuicao(unsigned char codigo[], int* pos, int idxVar, char varpcPrefix, int idxVarpc) {
  // atribuição variável-variável
  if (varpcPrefix == 'v') {
    RegInfo src = get_local_var_reg(idxVarpc);
    RegInfo dst = get_local_var_reg(idxVar);
    if (src.reg_code == -1 || dst.reg_code == -1) return;

    // Emite byte REX se necessário
    if (src.rex || dst.rex) {
      unsigned char rex = 0x40;
      if (src.rex) rex |= 0x04; // seta o bit R para reg de origem
      if (dst.rex) rex |= 0x01; // seta o bit B para reg de destino
      codigo[(*pos)++] = rex;
    }

    codigo[(*pos)++] = 0x89;  // mov
    // cálculo do byte ModRM
    codigo[(*pos)++] = 0xC0 + (src.reg_code << 3) + dst.reg_code;
  }
  // atribuição variável-parâmetro 
  else if (varpcPrefix == 'p') {
    RegInfo src = get_param_reg(idxVarpc);
    RegInfo dst = get_local_var_reg(idxVar);

    if (src.reg_code == -1 || dst.reg_code == -1) return;

    // Emite byte REX se necessário
    if (dst.rex) {
      codigo[(*pos)++] = 0x41;
    }

    codigo[(*pos)++] = 0x89;  // mov
    // cálculo do byte ModRM
    codigo[(*pos)++] = 0xC0 + (src.reg_code << 3) + dst.reg_code;

  } 
  // atribuição variável-imediato 
  else if (varpcPrefix == '$') {
    RegInfo dst = get_local_var_reg(idxVar);
    if (dst.reg_code == -1) return;

    // Emite byte REX se necessário
    if (dst.rex) {
      codigo[(*pos)++] = 0x41;
    }

    /**
     * Valor imediato para registrador: não usa byte ModRM
     * A regra é: 0xB8 + (número do registrador), seguido de 4 bytes do inteiro
     */
    codigo[(*pos)++] = 0xB8 + dst.reg_code;
    escreve_inteiro_em_4bytes_hex(codigo, pos, idxVarpc);
  }
}

/**
 * Escreve código de máquina de uma operação aritmética em SBas
 * vX = <vX | $num> op <vX | $num>
 */
static void escreve_operacao_aritmetica(unsigned char codigo[], int* pos, int idxVar, char varc1Prefix, int idxVarc1, char op, char varc2Prefix, int idxVarc2) {
  /**
   * For commutative operations, we swap the operands so we keep a single logic path
   */
  if ((op == '+' || op == '*') && varc1Prefix == '$' && varc2Prefix == 'v') {
    char tmpPrefix = varc1Prefix;
    int tmpIdx = idxVarc1;

    varc1Prefix = varc2Prefix;
    idxVarc1 = idxVarc2;
    varc2Prefix = tmpPrefix;
    idxVarc2 = tmpIdx;
  }

  
  /**
   * Escreve primeira instrução da operação aritmética:
   * mov <operandoDaEsquerda>, <variavelAtribuida>
   */
  if (varc1Prefix == 'v') {
    // registrador para registrador: mesma lógica do mov
    RegInfo src = get_local_var_reg(idxVarc1);
    RegInfo dst = get_local_var_reg(idxVar);
    if (src.reg_code == -1 || dst.reg_code == -1) return;

    // Escreve byte REX se necessário
    if (src.rex || dst.rex) {
      unsigned char rex = 0x40;
      if (src.rex) rex |= 0x04;
      if (dst.rex) rex |= 0x01;
      codigo[(*pos)++] = rex;
    }

    // Escreve byte `mov`
    codigo[(*pos)++] = 0x89;

    // Escreve ModRM
    codigo[(*pos)++] = 0xC0 + (src.reg_code << 3) + dst.reg_code;

  } else if (varc1Prefix == '$') {
    // valor imediato para registrador: não usa byte ModRM
    // A regra é: 0xB8 + (número do registrador), seguido de 4 bytes do inteiro
    RegInfo dst = get_local_var_reg(idxVar);
    if (dst.reg_code == -1) return;

    // Escreve byte REX se necessário (B = 1 para registrador estendido)
    if (dst.rex) {
      codigo[(*pos)++] = 0x41;
    }

    // Escreve a instrução mov em si para caso de constante -> registrador
    codigo[(*pos)++] = 0xB8 + dst.reg_code;
    escreve_inteiro_em_4bytes_hex(codigo, pos, idxVarc1);

  } else {
    fprintf(stderr, "Operação aritmética inválida!\n");
    return;
  }

  /**
   * Escreve segunda instrução da operação aritmética:
   * <op> <operandoDaDireita>, <variavelAtribuida>
   */
  if (varc2Prefix == 'v') {
    RegInfo src = get_local_var_reg(idxVarc2);
    RegInfo dst = get_local_var_reg(idxVar);
    if (src.reg_code == -1 || dst.reg_code == -1) return;

    // Exceção para multiplicação: inverte src/dst nos campos do byte REX
    if (op == '*') {
      char tmp = src.rex;
      src.rex = dst.rex;
      dst.rex = tmp;
    }

    // Escreve byte REX se necessário
    if (src.rex || dst.rex) {
      unsigned char rex = 0x40;
      if (src.rex) rex |= 0x04;
      if (dst.rex) rex |= 0x01;
      codigo[(*pos)++] = rex;
    }

    switch (op) {
      case '+': codigo[(*pos)++] = 0x01; break;
      case '-': codigo[(*pos)++] = 0x29; break;
      case '*': codigo[(*pos)++] = 0x0F; codigo[(*pos)++] = 0xAF; break;
      default:
        fprintf(stderr, "Operação aritmética inválida!\n");
        return;
    }

    // Exceção para multiplicação: inverte ordem reg/rm
    if (op == '*') {
      codigo[(*pos)++] = 0xC0 + (dst.reg_code << 3) + src.reg_code;
    } else {
      codigo[(*pos)++] = 0xC0 + (src.reg_code << 3) + dst.reg_code;
    }

  } else if (varc2Prefix == '$') {
    RegInfo dst = get_local_var_reg(idxVar);
    if (dst.reg_code == -1) return;

    // Emite REX
    if (dst.rex) {
      unsigned char rex = 0x40;
      rex |= 0x05;  // seta bit REX.R e REX.B
      codigo[(*pos)++] = rex;
    }    

    /**
     * Emite bytes das operações.
     * Os ifs para checar se o número cabe em um byte (-128 a 127) é uma otimização que a Intel criou
     * Valores imediatos pequenos (imm8) tem tamanho diferente das instruções comparado
     * aos imm32
     */
    switch (op) {
      case '+': {
        if (idxVarc2 >= -128 && idxVarc2 <= 127) {
          codigo[(*pos)++] = 0x83;
          codigo[(*pos)++] = 0xC0 + dst.reg_code;
          codigo[(*pos)++] = (unsigned char)(idxVarc2 & 0xFF);
        } else {
          codigo[(*pos)++] = 0x81;
          codigo[(*pos)++] = 0xC0 + dst.reg_code;
          escreve_inteiro_em_4bytes_hex(codigo, pos, idxVarc2);
        }
        break;
      }
      case '-': {
        if (idxVarc2 >= -128 && idxVarc2 <= 127) {
          codigo[(*pos)++] = 0x83;
          codigo[(*pos)++] = 0xE8 + dst.reg_code;
          codigo[(*pos)++] = (unsigned char)(idxVarc2 & 0xFF);
        } else {
          codigo[(*pos)++] = 0x81;
          codigo[(*pos)++] = 0xE8 + dst.reg_code;
          escreve_inteiro_em_4bytes_hex(codigo, pos, idxVarc2);
        }
        break;
      }
      case '*': {
        if (idxVarc2 >= -128 && idxVarc2 <= 127) {
          codigo[(*pos)++] = 0x6B;
          codigo[(*pos)++] = 0xC0 + dst.reg_code * 9;
          codigo[(*pos)++] = (unsigned char)(idxVarc2 & 0xFF);
        } else {
          codigo[(*pos)++] = 0x69;
          codigo[(*pos)++] = 0xC0 + dst.reg_code * 9;
          escreve_inteiro_em_4bytes_hex(codigo, pos, idxVarc2);
        }
        break;
      }
      default:
        fprintf(stderr, "Operação aritmética inválida: '%c'\n", op);
        return;
    }
  }
}

/**
 * Escreve a primeira parte (instrução) de um `iflez` SBas (cmpl $0, <registradorDaVariavel>)
 */
static void escreve_cmpl_de_desvio(unsigned char codigo[], int* pos, int varIndex) {
  RegInfo reg = get_local_var_reg(varIndex);
  if (reg.reg_code == -1) {
    fprintf(stderr, "escreve_cmpl_de_desvio: índice de variável inválido: v%d\n", varIndex);
    return;
  }

  // Emite REX
  if (reg.rex) {
    codigo[(*pos)++] = 0x41;
  }

  // Emite `cmp $0, <reg>`
  codigo[(*pos)++] = 0x83;
  codigo[(*pos)++] = 0xF8 + reg.reg_code;
  codigo[(*pos)++] = 0x00;

  // Emite jle <rel32>: coloca opcode e coloca os 4 bytes de offset depois
  codigo[(*pos)++] = 0x0F;
  codigo[(*pos)++] = 0x8E;
}

/**
 * Retorna o registrador associado a uma variável local v1–v5.
 *
 * O campo `reg_code` é o valor de 3 bits que representa o registrador no byte ModRM.
 * O campo `rex` indica se a variável usa um registrador estendido (r12d–r15d),
 * ou seja, se é necessário emitir um prefixo REX na instrução.
 *
 * Esta função **não** distingue se o registrador será usado
 * como origem ou destino — ou seja, se deve setar REX.R ou REX.B.
 * Essa decisão cabe à função chamadora, que sabe a posição do operando no ModRM.
 */
static RegInfo get_local_var_reg(int idx) {
  switch (idx) {
    case 1:
      // v1 -> ebx
      return (RegInfo){3, 0};
    case 2:
      // v2 -> r12d
      return (RegInfo){4, 1};
    case 3:
      // v3 -> r13d
      return (RegInfo){5, 1};
    case 4:
      // v4 -> r14d
      return (RegInfo){6, 1};
    case 5:
      // v5 -> r15d
      return (RegInfo){7, 1};
    default:
      fprintf(stderr, "get_local_var_reg: variável local inválida: v%d\n", idx);
      return (RegInfo){-1, -1};
  }
}

/**
 * Retorna o registrador associado a um parâmetro p1–p3.
 *
 * O campo `reg_code` indica qual registrador será usado para o parâmetro,
 * segundo a ABI System V vista em aula (edi, esi, edx).
 *
 * O campo `rex` é sempre 0 porque esses registradores não precisam do prefixo REX.
 *
 * Esta função **não determina** se o bit
 * REX.R ou REX.B deve ser usado — isso depende do contexto da instrução.
 */
static RegInfo get_param_reg(int idx) {
  switch (idx) {
    case 1:
      // p1 -> edi
      return (RegInfo){7, 0};
    case 2:
      // p2 -> esi
      return (RegInfo){6, 0};
    case 3:
      // p3 -> edx
      return (RegInfo){2, 0};
    default:
      fprintf(stderr, "get_param_reg: parâmetro inválido: p%d\n", idx);
      return (RegInfo){-1, -1};
  }
}
