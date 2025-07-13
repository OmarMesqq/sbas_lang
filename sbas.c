#include "sbas.h"

#include <stdio.h>
#include <stdlib.h>
#include <sys/mman.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>

#define DEBUG
#define RED "\033[31m"
#define RESET_COLOR "\033[0m"
#define MAX_LINES 30
#define MAX_CODE_SIZE 1024

/**
 * Represents a parsed line in SBas source code
 * 
 * Fields:
 * - line: current line
 * - offset: start of current line in the buffer
 */
typedef struct {
  unsigned char line;
  int offset;
} SymbolTable;

/**
 * Represents an entry in the relocation table for conditional jump instructions
 * 
 * Fields:
 * - lineTarget: line to jump to
 * - offset: position in the buffer where the jump offset needs to be patched to jump to `lineTarget`
 */
typedef struct {
  unsigned char lineTarget;
  int offset;
} RelocationTable;

/**
 * Represents a general purpose register.
 * 
 * Fields:
 * - reg_code: numeric code for register (0–7).
 * - rex: boolean flag indicating the need for a REX prefix byte necessary for extended registers (8–15).
 */
typedef struct {
  int reg_code;
  char rex;
} RegInfo;


static void error(const char* msg, int line);
static void emit_prologue(unsigned char code[], int* pos);
static void zero_initialize_registers(unsigned char code[], int* pos);
static void save_callee_saved_registers(unsigned char code[], int* pos);
static void restore_callee_saved_registers(unsigned char code[], int* pos);
static void emit_epilogue(unsigned char code[], int* pos);
static void emit_integer_in_hex(unsigned char code[], int* pos, int integer);
static void emit_constant_literal_return(unsigned char code[], int* pos, int returnValue);
static void emit_variable_return(unsigned char code[], int* pos, int varIdx);
static void emit_attribution(unsigned char code[], int* pos, int idxVar, char varpcPrefix, int idxVarpc);
static void emit_arithmetic_operation(unsigned char code[], int* pos, int idxVar, char varc1Prefix, int idxVarc1, char op, char varc2Prefix, int idxVarc2);
static void emit_cmp_jump_instruction(unsigned char code[], int* pos, int varIndex);
static void emit_rex_byte(unsigned char code[], int* pos, char src_rex, char dst_rex);
static void emit_modrm(unsigned char code[], int* pos, int src_reg_code, int dst_reg_code);
static void emit_mov(unsigned char code[], int* pos);
static void emit_mov_imm(unsigned char code[], int* pos, int dst_reg_code, int integer);
static RegInfo get_local_var_reg(int idx);
static RegInfo get_param_reg(int idx);
static void* alloc_writable_buffer(size_t size);
static int make_buffer_executable(void* ptr, size_t size);
static void print_symbol_table(SymbolTable* st, int lines);
static void print_relocation_table(RelocationTable* rt, int relocCount);

/**
 * Compiles a SBas function described in a .sbas file at
 * the open `FILE*` handle `f`
 */
funcp sbasCompile(FILE* f) {
  unsigned line = 1;  // in .sbas file
  char lineBuffer[256]; // for parsing the text lines
  int pos = 0;         // byte position in the buffer
  int relocCount = 0;  // how many lines should have jump offsets patched in the 2nd pass

  SymbolTable* st = calloc((MAX_LINES + 1), sizeof(SymbolTable));
  RelocationTable* rt = calloc((MAX_LINES + 1), sizeof(RelocationTable));
  if (!st || !rt) {
    fprintf(stderr, "Failed to alloc symbol and/or relocation table!\n");
    return NULL;
  }

  unsigned char* code = alloc_writable_buffer(MAX_CODE_SIZE);
  if (!code) {
    fprintf(stderr, "Failed to alloc writable memory.\n");
    return NULL;
  }


  emit_prologue(code, &pos);
  save_callee_saved_registers(code, &pos);
  zero_initialize_registers(code, &pos);

  /**
   * First pass: emit most instructions and leave 4-byte placeholders for jumps
   */
  while (fgets(lineBuffer, sizeof(lineBuffer), f) && line <= MAX_LINES) {
    // Skip comment lines, but still increase the parsed lines counter
    if (lineBuffer[0] == '/') {
      line++;
      continue;
    }
    
    st[line].line = line;
    st[line].offset = pos;

    switch (lineBuffer[0]) {
      case 'r': { /* return */
        int varc;

        // local variable return (ret vX)
        if (sscanf(lineBuffer, "ret v%d", &varc) == 1) {
          emit_variable_return(code, &pos, varc);
        }
        // constant literal return (ret $snum)
        else if (sscanf(lineBuffer, "ret $%d", &varc) == 1) {
          emit_constant_literal_return(code, &pos, varc);
        }
        // syntax error!
        else {
          error("invalid 'ret' command: expected 'ret <var|$int>", line);
          return NULL;
        }

        break;
      }
      case 'v': { /* attribution and arithmetic operation */
        int idxVar;
        char operator;
        if (sscanf(lineBuffer, "v%d %c", &idxVar, &operator) != 2) {
          error("invalid command: expected attribution (vX: varpc) or arithmetic operation (vX = varc op varc)", line);
          return NULL;
        }

        // Only 5 locals allowed for now (v1, v2, v3, v4, v5)
        if (idxVar < 1 || idxVar > 5) {
          error("invalid local variable index: only 5 locals are allowed.", line);
          return NULL;
        }

        // attribution
        if (operator == ':') {
          char varpcPrefix;
          int idxVarpc;
          if (sscanf(lineBuffer, "v%d : %c%d", &idxVar, &varpcPrefix, &idxVarpc) != 3) {
            error("invalid attribution: expected 'vX: <vX|pX|$num>'", line);
            return NULL;
          }
          emit_attribution(code, &pos, idxVar, varpcPrefix, idxVarpc);
        }
        // arithmetic operation
        else if (operator == '=') {
          int idxVar;
          char varc1Prefix;
          int idxVarc1;
          char op;
          char varc2Prefix;
          int idxVarc2;

          if (sscanf(lineBuffer, "v%d = %c%d %c %c%d", &idxVar, &varc1Prefix, &idxVarc1, &op, &varc2Prefix, &idxVarc2) != 6) {
            error("invalid arithmetic operation: expected 'vX = <vX|$num> op <vX|$num>'", line);
            return NULL;
          }

          if (op != '+' && op != '-' && op != '*') {
            error("invalid arithmetic operation: only addition (+), subtraction (-), and multiplication (*) allowed.", line);
            return NULL;
          }

          emit_arithmetic_operation(code, &pos, idxVar, varc1Prefix, idxVarc1, op, varc2Prefix, idxVarc2);
        }

        break;
      }
      case 'i': { /* conditional jump */
        int varIndex;
        unsigned lineTarget;
        if (sscanf(lineBuffer, "iflez v%d %u", &varIndex, &lineTarget) != 2) {
          error("invalid 'iflez' command: expected 'iflez vX line'", line);
          return NULL;
        }

        emit_cmp_jump_instruction(code, &pos, varIndex);

        // Mark current line to be resolved in patching step
        rt[relocCount].lineTarget = lineTarget;
        rt[relocCount].offset = pos;
        relocCount++;

        // Emit 4-byte placeholder
        code[pos++] = 0x00;
        code[pos++] = 0x00;
        code[pos++] = 0x00;
        code[pos++] = 0x00;

        break;
      }
      default:
        error("unknown SBas command", line);
        return NULL;
    }
    line++;
    fscanf(f, " ");
  }

  /**
   * Second pass: fills 4-byte placeholder with offsets
   */
  for (int i = 0; i < relocCount; i++) {
    // get start of the line to jump to
    int targetOffset = st[rt[i].lineTarget].offset;
    // get the location in buffer to jump from (the 4-byte placeholder to fill)
    int jumpFrom = rt[i].offset;

    /**
     * The CPU processes jumps as:
     * target_address = next_instruction_address + offset
     * 
     * The `jle` instruction is 6 bytes wide: (2 for opcode, 4 for relative offset)
     * 
     * Currently, we are at the offset field (`jumpFrom`) since the 2 bytes 
     * corresponding to the opcode were processed. Now, we add 4 to advance through the
     * 4 bytes of relative offset (`jumpFrom`) to get to the next instruction.
     */
    int rel32 = targetOffset - (jumpFrom + 4);
    emit_integer_in_hex(code, &jumpFrom, rel32);
  }

  #ifdef DEBUG
  printf("sbasCompile wrote %d bytes in buffer.\n", pos);
  printf("Parsed %d lines.\n", line);
  printf("%d lines were patched.\n", relocCount);
  print_symbol_table(st, line);
  print_relocation_table(rt, relocCount);
  #endif

  free(st);
  free(rt);

  int res = make_buffer_executable(code, MAX_CODE_SIZE);
  if (res == -1) {
    return NULL;
  }

  return (funcp)code;
}

/**
 * Frees the executable buffer of a SBas function `sbasFunc`
 */
void sbasCleanup(funcp sbasFunc) {
  munmap((void*) sbasFunc, MAX_CODE_SIZE);
}

static void error(const char* msg, int line) { 
  fprintf(stderr, "%s[line %d in .sbas file]: %s%s\n", RED, line, msg, RESET_COLOR);
}

/**
 * Starts an x86-64 function. Saves previous frame base pointer
 * and configure current stack frame.
 */
static void emit_prologue(unsigned char code[], int* pos) {
  // pushq %rbp
  code[*pos] = 0x55;
  (*pos)++;

  // movq %rsp, %rbp
  code[*pos] = 0x48;
  (*pos)++;
  code[*pos] = 0x89;
  (*pos)++;
  code[*pos] = 0xe5;
  (*pos)++;
}

/**
 * Even though uninitialized variables are not accepted per
 * SBas specification, I zero initialized all local variable registers.
 */
static void zero_initialize_registers(unsigned char code[], int* pos) {
  // xorl %ebx,%ebx
  code[*pos] = 0x31;
  (*pos)++;
  code[*pos] = 0xdb;
  (*pos)++;

  // xorl  %r12d,%r12d
  code[*pos] = 0x45;
  (*pos)++;
  code[*pos] = 0x31;
  (*pos)++;
  code[*pos] = 0xe4;
  (*pos)++;


  // xorl  %r13d, %r13d
  code[*pos] = 0x45;
  (*pos)++;
  code[*pos] = 0x31;
  (*pos)++;
  code[*pos] = 0xed;
  (*pos)++;

  // xorl  %r14d, %r14d
  code[*pos] = 0x45;
  (*pos)++;
  code[*pos] = 0x31;
  (*pos)++;
  code[*pos] = 0xf6;
  (*pos)++;

  // xorl  %r15d, %r15d
  code[*pos] = 0x45;
  (*pos)++;
  code[*pos] = 0x31;
  (*pos)++;
  code[*pos] = 0xff;
  (*pos)++;

}

/**
 * I mapped the SBas local variables to callee-saved registers. As such,
 * they have to be saved in the stack frame for later restoration.
 * v1 - ebx
 * v2 - r12d
 * v3 - r13d
 * v4 - r14d
 * v5 - r15d
 */
static void save_callee_saved_registers(unsigned char code[], int* pos) {
  // subq $48, %rsp
  code[*pos] = 0x48;
  (*pos)++;
  code[*pos] = 0x83;
  (*pos)++;
  code[*pos] = 0xEC;
  (*pos)++;
  code[*pos] = 0x30;
  (*pos)++;

  // movq %rbx, -8(%rbp)
  code[*pos] = 0x48;
  (*pos)++;
  code[*pos] = 0x89;
  (*pos)++;
  code[*pos] = 0x5D;
  (*pos)++;
  code[*pos] = 0xF8;
  (*pos)++;

  // movq %r12, -16(%rbp)
  code[*pos] = 0x4C;
  (*pos)++;
  code[*pos] = 0x89;
  (*pos)++;
  code[*pos] = 0x65;
  (*pos)++;
  code[*pos] = 0xF0;
  (*pos)++;

  // movq %r13, -24(%rbp)
  code[*pos] = 0x4C;
  (*pos)++;
  code[*pos] = 0x89;
  (*pos)++;
  code[*pos] = 0x6D;
  (*pos)++;
  code[*pos] = 0xE8;
  (*pos)++;

  // movq %r14, -32(%rbp)
  code[*pos] = 0x4C;
  (*pos)++;
  code[*pos] = 0x89;
  (*pos)++;
  code[*pos] = 0x75;
  (*pos)++;
  code[*pos] = 0xE0;
  (*pos)++;

  // movq %r15, -40(%rbp)
  code[*pos] = 0x4C;
  (*pos)++;
  code[*pos] = 0x89;
  (*pos)++;
  code[*pos] = 0x7D;
  (*pos)++;
  code[*pos] = 0xD8;
  (*pos)++;
}

/**
 * Restore callee-saved registers at the end of an SBas function
 */
static void restore_callee_saved_registers(unsigned char code[], int* pos) {
  // movq -8(%rbp), %rbx
  code[*pos] = 0x48;
  (*pos)++;
  code[*pos] = 0x8B;
  (*pos)++;
  code[*pos] = 0x5D;
  (*pos)++;
  code[*pos] = 0xF8;
  (*pos)++;

  // movq -16(%rbp), %r12
  code[*pos] = 0x4C;
  (*pos)++;
  code[*pos] = 0x8B;
  (*pos)++;
  code[*pos] = 0x65;
  (*pos)++;
  code[*pos] = 0xF0;
  (*pos)++;

  // movq -24(%rbp), %r13
  code[*pos] = 0x4C;
  (*pos)++;
  code[*pos] = 0x8B;
  (*pos)++;
  code[*pos] = 0x6D;
  (*pos)++;
  code[*pos] = 0xE8;
  (*pos)++;

  // movq -32(%rbp), %r14
  code[*pos] = 0x4C;
  (*pos)++;
  code[*pos] = 0x8B;
  (*pos)++;
  code[*pos] = 0x75;
  (*pos)++;
  code[*pos] = 0xE0;
  (*pos)++;

  // movq -40(%rbp), %r15
  code[*pos] = 0x4C;
  (*pos)++;
  code[*pos] = 0x8B;
  (*pos)++;
  code[*pos] = 0x7D;
  (*pos)++;
  code[*pos] = 0xD8;
  (*pos)++;
}

/**
 * Undoes current stack frame: emits leave and ret
 */
static void emit_epilogue(unsigned char code[], int* pos) {
  //leave
  code[*pos] = 0xc9;
  (*pos)++;

  // ret
  code[*pos] = 0xc3;
  (*pos)++;
}

/**
 * Receives a signed integer (32 bits on x86-64) in base 10 and writes it
 * in Little Endian hexadecimal in the buffer. Used for immediate values and jump offsets.
 */
static void emit_integer_in_hex(unsigned char code[], int* pos, int integer) {
  code[*pos] = integer & 0xFF;
  (*pos)++;
  code[*pos] = (integer >> 8) & 0xFF;
  (*pos)++;
  code[*pos] = (integer >> 16) & 0xFF;
  (*pos)++;
  code[*pos] = (integer >> 24) & 0xFF;
  (*pos)++;
}

/**
 * Emits machine code for returning an immediate value (movl $imm32, %eax)
 */
static void emit_constant_literal_return(unsigned char code[], int* pos, int returnValue) {
  code[*pos] = 0xb8;  // movl ..., %eax
  (*pos)++;

  emit_integer_in_hex(code, pos, returnValue);

  restore_callee_saved_registers(code, pos);
  emit_epilogue(code, pos);
}

/**
 * Emits machine code for returning an SBas variable (v1 through v5)
 */
static void emit_variable_return(unsigned char code[], int* pos, int varIdx) {
  RegInfo reg = get_local_var_reg(varIdx);
  if (reg.reg_code == -1) return;

  emit_rex_byte(code, pos, reg.rex, 0);

  emit_mov(code, pos);

  // ModRM with `r/m` set to zero: target register is %eax
  emit_modrm(code, pos, reg.reg_code, 0);

  restore_callee_saved_registers(code, pos);
  emit_epilogue(code, pos);
}

/**
 * Emits machine code for a SBas attribution:
 * vX: <vX|pX|$num>
 */
static void emit_attribution(unsigned char code[], int* pos, int idxVar, char varpcPrefix, int idxVarpc) {
  // att var to var
  if (varpcPrefix == 'v') {
    RegInfo src = get_local_var_reg(idxVarpc);
    RegInfo dst = get_local_var_reg(idxVar);
    if (src.reg_code == -1 || dst.reg_code == -1) return;

    emit_rex_byte(code, pos, src.rex, dst.rex);
    emit_mov(code, pos);
    emit_modrm(code, pos, src.reg_code, dst.reg_code);
  }
  // att var param
  else if (varpcPrefix == 'p') {
    RegInfo src = get_param_reg(idxVarpc);
    RegInfo dst = get_local_var_reg(idxVar);
    if (src.reg_code == -1 || dst.reg_code == -1) return;

    emit_rex_byte(code, pos, 0, dst.rex);    
    emit_mov(code, pos);
    emit_modrm(code, pos, src.reg_code, dst.reg_code);
  } 
  // att var imm
  else if (varpcPrefix == '$') {
    RegInfo dst = get_local_var_reg(idxVar);
    if (dst.reg_code == -1) return;

    emit_rex_byte(code, pos, 0, dst.rex);
    emit_mov_imm(code, pos, dst.reg_code, idxVarpc);
  }
}

/**
 * Emit machine code for a SBas arithmetic operation:
 * vX = <vX | $num> op <vX | $num>
 */
static void emit_arithmetic_operation(unsigned char code[], int* pos, int idxVar, char varc1Prefix, int idxVarc1, char op, char varc2Prefix, int idxVarc2) {
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
   * Emits first instruction of an arithmetic operation:
   * mov <leftOperand>, <attributedVar>
   */
  if (varc1Prefix == 'v') {
    RegInfo src = get_local_var_reg(idxVarc1);
    RegInfo dst = get_local_var_reg(idxVar);
    if (src.reg_code == -1 || dst.reg_code == -1) return;

    emit_rex_byte(code, pos, src.rex, dst.rex);
    emit_mov(code, pos);
    emit_modrm(code, pos, src.reg_code, dst.reg_code);

  } else if (varc1Prefix == '$') {
    RegInfo dst = get_local_var_reg(idxVar);
    if (dst.reg_code == -1) return;

    emit_rex_byte(code, pos, 0, dst.rex);

    emit_mov_imm(code, pos, dst.reg_code, idxVarc1);
  } else {
    fprintf(stderr, "emit_arithmetic_operation: invalid varc1Prefix: %c\n", varc1Prefix);
    return;
  }

  /**
   * Emits second instruction of an arithmetic operation:
   * <op> <rightOperand>, <attributedVar>
   */
  if (varc2Prefix == 'v') {
    RegInfo src = get_local_var_reg(idxVarc2);
    RegInfo dst = get_local_var_reg(idxVar);
    if (src.reg_code == -1 || dst.reg_code == -1) return;

    // Special case for multiplication: src and dst fields are swapped in REX byte
    if (op == '*') {
      char tmp = src.rex;
      src.rex = dst.rex;
      dst.rex = tmp;
    }

    emit_rex_byte(code, pos, src.rex, dst.rex);

    switch (op) {
      case '+': code[(*pos)++] = 0x01; break;
      case '-': code[(*pos)++] = 0x29; break;
      case '*': code[(*pos)++] = 0x0F; code[(*pos)++] = 0xAF; break;
      default:
        fprintf(stderr, "emit_arithmetic_operation: invalid operation: %c\n", op);
        return;
    }

    // Special case for multiplication: reg and r/m are swapped in ModRM byte
    if (op == '*') {
      int temp = dst.reg_code;
      dst.reg_code = src.reg_code;
      src.reg_code = temp;
    }
    emit_modrm(code, pos, src.reg_code, dst.reg_code);

  } else if (varc2Prefix == '$') {
    RegInfo dst = get_local_var_reg(idxVar);
    if (dst.reg_code == -1) return;

    if (dst.rex) {
      emit_rex_byte(code, pos, 1, dst.rex);
    }    

    /**
     * Emit operations.
     * The ifs for checking if the number fits in a byte (-128 to 127) are due to
     * the possibility of emitting bytes for large values (imm32) and smaller ones (imm8).
     * Not sure if this is optimal though.
     */
    switch (op) {
      case '+': {
        if (idxVarc2 >= -128 && idxVarc2 <= 127) {
          code[(*pos)++] = 0x83;
          code[(*pos)++] = 0xC0 + dst.reg_code;
          code[(*pos)++] = (unsigned char)(idxVarc2 & 0xFF);
        } else {
          code[(*pos)++] = 0x81;
          code[(*pos)++] = 0xC0 + dst.reg_code;
          emit_integer_in_hex(code, pos, idxVarc2);
        }
        break;
      }
      case '-': {
        if (idxVarc2 >= -128 && idxVarc2 <= 127) {
          code[(*pos)++] = 0x83;
          code[(*pos)++] = 0xE8 + dst.reg_code;
          code[(*pos)++] = (unsigned char)(idxVarc2 & 0xFF);
        } else {
          code[(*pos)++] = 0x81;
          code[(*pos)++] = 0xE8 + dst.reg_code;
          emit_integer_in_hex(code, pos, idxVarc2);
        }
        break;
      }
      case '*': {
        if (idxVarc2 >= -128 && idxVarc2 <= 127) {
          code[(*pos)++] = 0x6B;
          code[(*pos)++] = 0xC0 + dst.reg_code * 9;
          code[(*pos)++] = (unsigned char)(idxVarc2 & 0xFF);
        } else {
          code[(*pos)++] = 0x69;
          code[(*pos)++] = 0xC0 + dst.reg_code * 9;
          emit_integer_in_hex(code, pos, idxVarc2);
        }
        break;
      }
      default:
        fprintf(stderr, "emit_arithmetic_operation: invalid operation: %c\n", op);
        return;
    }
  }
}

/**
 * Writes first instruction of a SBas conditional jump (`iflez`):
 * cmpl $0, <variableRegister>
 * as well as the jle conditional jump opcode
 */
static void emit_cmp_jump_instruction(unsigned char code[], int* pos, int varIndex) {
  RegInfo reg = get_local_var_reg(varIndex);
  if (reg.reg_code == -1) return;

  emit_rex_byte(code, pos, 0, reg.rex);
  
  // Emit `cmp $0, <reg>`
  code[(*pos)++] = 0x83;
  code[(*pos)++] = 0xF8 + reg.reg_code;
  code[(*pos)++] = 0x00;

  // Emit jle
  code[(*pos)++] = 0x0F;
  code[(*pos)++] = 0x8E;
}

/**
 * Returns the register where a local variable v1 through v5 is stored
 *
 * The `reg_code` field is a 3 bit value which represents the register code in the ModRM byte
 * The `rex` fields indicates if such register is extended (r12d - r15d) i.e. in 
 * order to access it, a REX prefix byte is necessary.
 *
 * The function has no info regarding the usage of the register as source or target. 
 * The caller knows whether the register is the source (`reg`) or the target (`r/m`) of
 * the operation in the ModRM byte, and, as such, will set the REX.R or REX.B bits for 
 * source and target respectively.
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
      fprintf(stderr, "get_local_var_reg: invalid local variable index: v%d\n", idx);
      return (RegInfo){-1, -1};
  }
}

/**
 * Returns the register where a parameter p1 through p3 is sent to a function
 * according to the System V ABI
 * Retorna o registrador associado a um parâmetro p1–p3.
 *
 * The `rex` field here is always zero because these registers are not extended.
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
      fprintf(stderr, "get_param_reg: invalid parameter index: p%d\n", idx);
      return (RegInfo){-1, -1};
  }
}

/**
 * Allocates a RW buffer for emitting machine code
 * corresponding to SBas code semantics
 */
static void* alloc_writable_buffer(size_t size) {
  // Round to page size
  size_t pagesize = sysconf(_SC_PAGESIZE);

  size_t alloc_size = ((size + pagesize - 1) / pagesize) * pagesize;
  void* ptr = mmap(NULL, alloc_size,
                   PROT_READ | PROT_WRITE,
                   MAP_PRIVATE | MAP_ANONYMOUS,
                   -1, 0);
  if (ptr == MAP_FAILED) {
    fprintf(stderr, "alloc_writable_buffer: failed to mmap writable buffer.\n");
    return NULL;
  }

  return ptr;
}

/**
 * Drops write flag of SBas code buffer after done emitting.
 * This enforces W^X security policy
 */
static int make_buffer_executable(void* ptr, size_t size) {
  size_t pagesize = sysconf(_SC_PAGESIZE);
  size_t alloc_size = ((size + pagesize - 1) / pagesize) * pagesize;

  // change protection to R+X (drop Write)
  if (mprotect(ptr, alloc_size, PROT_READ | PROT_EXEC) != 0) {
    fprintf(stderr, "make_buffer_executable: failed to set buffer to R+X through mprotect.\n");
    return -1;
  }

  return 0;
}

static void print_symbol_table(SymbolTable* st, int lines) {
  printf("----- START SYMBOL TABLE -----\n");
  printf("%-14s %s\n", "LINE", "START OFFSET (dec)");
  for (int i = 1; i < lines; i++) {
    if (st[i].line == 0) continue; 
    printf("%-14d %d\n", st[i].line, st[i].offset);
  }
  printf("----- END SYMBOL TABLE -----\n");
}

static void print_relocation_table(RelocationTable* rt, int relocCount) {
  printf("----- START RELOCATION TABLE -----\n");
  printf("%-20s %s\n", "TARGET LINE", "PATCH OFFSET (dec)");
  for (int i = 0; i < relocCount; i++) {
    printf("%-20d %d\n", rt[i].lineTarget, rt[i].offset);
  }
  printf("----- END RELOCATION TABLE -----\n");
}

/**
 * Emits, if necessary, the prefix REX byte.
 * This is mandatory for accessing extended registers like r12d through r15d
 * 
 * If `src_rex` is true, the bit 2, called R, is set, extending the
 * `reg` field in ModRM (source register).
 * 
 * If `dst_rex` is true, the bit 0, called B, is set, extending the
 * `r/m` field in ModRM (target register).
 * 
 * If both are true, both bits are sets and the corresponding fields are extended
 */
static void emit_rex_byte(unsigned char code[], int* pos, char src_rex, char dst_rex) {
  unsigned char rex = 0x40;
  
  if (src_rex) {
    // REX byte with bit R set (for source register)
    rex |= 0x04;
  }
  if (dst_rex) {
    // REX byte with bit B set (for target register)
    rex |= 0x01;
  }
  code[(*pos)++] = rex;
}

static void emit_mov(unsigned char code[], int* pos) {
  code[*pos] = 0x89;
  (*pos)++; 
}

/**
 * The ModRM byte tells the CPU what are the operands involved in an operation.
 * It is built like this:
 * mod |  reg |  r/m
 * bb  | bbb  |  bbb
 * 
 * The `mod` field is set to 11 which means the operation is between two registers
 * The 3 bit `reg` field maps to the SOURCE register of the operation
 * whereas the `r/m` field (also 3 bit wide) maps to the TARGET register
 */
static void emit_modrm(unsigned char code[], int* pos, int src_reg_code, int dst_reg_code) {
  code[(*pos)++] = 0xC0 + (src_reg_code << 3) + dst_reg_code;
}

/**
 * Emits `mov` $imm, <reg>.
 * Does not use ModRM byte, rather:
 * 0xB8 + dst_reg_code followed by 4 bytes of imm
 */
static void emit_mov_imm(unsigned char code[], int* pos, int dst_reg_code, int integer) {
  code[(*pos)++] = 0xB8 + dst_reg_code;
  emit_integer_in_hex(code, pos, integer);
}
