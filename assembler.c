#include "assembler.h"

#include <stdio.h>

#include "config.h"
#include "utils.h"

#define BUFFER_SIZE 128  // length of a parsed line and of an error message

static void emit_instruction(unsigned char code[], int* pos, Instruction* inst);
static void emit_prologue(unsigned char code[], int* pos);
static void save_callee_saved_registers(unsigned char code[], int* pos);
static void emit_return(unsigned char code[], int* pos, char retType,
                        int returnValue);
static void emit_attribution(unsigned char code[], int* pos, int idxVar,
                             char varpcPrefix, int idxVarpc);
static void emit_arithmetic_operation(unsigned char code[], int* pos,
                                      int idxVar, char varc1Prefix,
                                      int idxVarc1, char op, char varc2Prefix,
                                      int idxVarc2);
static void emit_cmp(unsigned char code[], int* pos, int varIndex);
static void emit_jle(unsigned char code[], int* pos);
static void emit_rex_byte(unsigned char code[], int* pos, char src_rex,
                          char dst_rex);
static void emit_modrm(unsigned char code[], int* pos, int mode, int source,
                       int dest);
static inline void emit_mov(unsigned char code[], int* pos);
static void emit_mov_imm(unsigned char code[], int* pos, int dst_reg_code,
                         int integer);
static void restore_callee_saved_registers(unsigned char code[], int* pos);
static void emit_epilogue(unsigned char code[], int* pos);

static RegInfo new_get_local_var_reg(int idx);
static RegInfo get_local_var_reg(int idx);
static RegInfo get_param_reg(int idx);

/**
 * Receives an **open** file handle of the SBas file and
 * attempts to write corresponding logic in x86-64 machine code to a buffer
 *
 * @param code writable buffer
 * @param f **open** file handle to SBas file
 * @param lt pointer to a line table struct
 * @param rt pointer to a relocation table struct
 * @param relocCount pointer to a counter for tracking lines with jumps
 *
 * @returns 0 on success, -1 on failure
 */
char sbasAssemble(unsigned char* code, FILE* f, LineTable* lt,
                  RelocationTable* rt, int* relocCount) {
  unsigned line = 1;                       // count in the SBas file
  char lineBuffer[BUFFER_SIZE] = {0};      // a line in the SBas file
  char errorMsgBuffer[BUFFER_SIZE] = {0};  // a compilation error message
  int pos = 0;                             // byte position in the buffer
  char retFound = 0;                       // every SBas function MUST return

  emit_prologue(code, &pos);
  save_callee_saved_registers(code, &pos);

  while (fgets(lineBuffer, sizeof(lineBuffer), f)) {
    trimLeadingSpaces(lineBuffer);

    if (lineBuffer[0] == ' ' || lineBuffer[0] == '\n' ||
        lineBuffer[0] == '\0' || lineBuffer[0] == '/') {
      line++;
      continue;
    }

    if (line > MAX_LINES) {
      fprintf(stderr,
              "sbasCompile: the provided SBas file exceeds MAX_LINES (%d)!\n",
              MAX_LINES);
      return -1;
    }

    lt[line].line = line;
    lt[line].offset = pos;

    switch (lineBuffer[0]) {
      case 'r': { /* return */
        int varc;
        char
            retType;  // 'v' for variable return, '$' for immediate value return

        if (sscanf(lineBuffer, "ret %c%d", &retType, &varc) != 2 ||
            (retType != 'v' && retType != '$')) {
          compilationError(
              "sbasCompile: invalid 'ret' command: expected 'ret <var|$int>",
              line);
          return -1;
        }

        emit_return(code, &pos, retType, varc);
        if (!retFound) retFound = 1;

        break;
      }
      case 'v': { /* attribution and arithmetic operation */
        int idxVar;
        char operator;

        if (sscanf(lineBuffer, "v%d %c", &idxVar, &operator) != 2) {
          compilationError(
              "sbasCompile: invalid command: expected attribution (vX: varpc) "
              "or arithmetic operation (vX = varc op varc)",
              line);
          return -1;
        }

        if (idxVar < 1 || idxVar > 5) {
          snprintf(errorMsgBuffer, BUFFER_SIZE,
                   "sbasCompile: invalid local variable index %d. Only v1 "
                   "through v5 are allowed.",
                   idxVar);
          compilationError(errorMsgBuffer, line);
          return -1;
        }

        if (operator != ':' && operator != '=') {
          snprintf(errorMsgBuffer, BUFFER_SIZE,
                   "sbasCompile: invalid operator %c. Only attribution (:) and "
                   "arithmetic operation (=) are supported.",
                   operator);
          compilationError(errorMsgBuffer, line);
          return -1;
        }

        // attribution
        if (operator == ':') {
          char varpcPrefix;
          int idxVarpc;
          if (sscanf(lineBuffer, "v%d : %c%d", &idxVar, &varpcPrefix,
                     &idxVarpc) != 3) {
            compilationError(
                "sbasCompile: invalid attribution: expected 'vX: <vX|pX|$num>'",
                line);
            return -1;
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
          char remaining[BUFFER_SIZE] = {
              0};  // used in scanset to detect extra operands/operators

          if (sscanf(lineBuffer, "v%d = %c%d %c %c%d %127[^\n]", &idxVar,
                     &varc1Prefix, &idxVarc1, &op, &varc2Prefix, &idxVarc2,
                     remaining) != 6) {
            compilationError(
                "sbasCompile: invalid arithmetic operation: expected 'vX = "
                "<vX|$num> op <vX|$num>'",
                line);
            return -1;
          }

          if (op != '+' && op != '-' && op != '*') {
            snprintf(
                errorMsgBuffer, BUFFER_SIZE,
                "sbasCompile: invalid arithmetic operation %c. Only addition "
                "(+), subtraction (-), and multiplication (*) allowed.",
                op);
            compilationError(errorMsgBuffer, line);
            return -1;
          }

          emit_arithmetic_operation(code, &pos, idxVar, varc1Prefix, idxVarc1,
                                    op, varc2Prefix, idxVarc2);
        }

        break;
      }
      case 'i': { /* conditional jump */
        int varIndex;
        unsigned lineTarget;

        if (sscanf(lineBuffer, "iflez v%d %u", &varIndex, &lineTarget) != 2) {
          compilationError(
              "sbasCompile: invalid 'iflez' command: expected 'iflez vX line'",
              line);
          return -1;
        }

        emit_cmp(code, &pos, varIndex);
        emit_jle(code, &pos);

        // Mark current line to be resolved in patching step
        rt[*relocCount].lineTarget = lineTarget;
        rt[*relocCount].offset = pos;
        (*relocCount)++;

        // Emit 4-byte placeholder
        code[pos++] = 0x00;
        code[pos++] = 0x00;
        code[pos++] = 0x00;
        code[pos++] = 0x00;

        break;
      }
      default: {
        compilationError("sbasCompile: unknown SBas command", line);
        return -1;
      }
    }
    line++;
  }

  if (!retFound) {
    fprintf(stderr,
            "sbasCompile: SBas function doesn't include 'ret'. Aborting!\n");
    return -1;
  }

#ifdef DEBUG
  printf("sbasCompile: processed %d lines, writing %d bytes in buffer\n",
         line - 1, pos);
  printf("sbasCompile: %d lines were patched\n", *relocCount);
  printLineTable(lt, line);
  printRelocationTable(rt, *relocCount);
#endif
  return 0;
}

/**
 * Starts an x86-64 function. Saves previous frame base pointer
 * and configures current stack frame.
 */
static void emit_prologue(unsigned char code[], int* pos) {
  // pushq %rbp
  code[*pos] = 0x55;
  (*pos)++;

  // movq %rsp, %rbp
  Instruction initStackFrame = {0};
  initStackFrame.opcode = 0x89; // mov 
  initStackFrame.is_64bit = 1;

  initStackFrame.use_modrm = 1;
  initStackFrame.mod = 3; // between registers
  initStackFrame.reg = 4; // from rsp
  initStackFrame.rm = 5;  // to rbp

  emit_instruction(code, pos, &initStackFrame);
}

/**
 * Decrements the stack pointer by 48 bytes and saves the initial values
 * of the callee-saved registers in the stack frame.
 *
 * Since SBas local variables (v1 through v5) are mapped to callee-saved
 * registers, the latter's initial values have to be saved for later restoration
 * as to comply with the System V ABI.
 *
 * The extra 8 bytes may be unused, but are crucial as they guarantee the stack
 * is aligned at a 16-byte boundary (another ABI requirement)
 */
static void save_callee_saved_registers(unsigned char code[], int* pos) {
  // adjust stack pointer: subq $48, %rsp
  Instruction sub = {0};
  sub.opcode = 0x83;     // 0x83 = Arithmetic with byte immediate
  sub.is_64bit = 1;      
  sub.use_modrm = 1;     
  sub.mod = 3;           // 11 = Register Direct Mode
  sub.reg = 5;           // 5 = Opcode Extension for SUB
  sub.rm = 4;            // 4 = RSP Register ID
  sub.use_imm = 1;
  sub.imm_size = 1;      // 1 byte immediate
  sub.immediate = 48;    // Value 48
  emit_instruction(code, pos, &sub);

  // 2. Save Registers to Stack
  // Create a template for "MOV Reg to Memory[RBP + Offset]"
  Instruction mov = {0};
  mov.opcode = 0x89;     // MOV r/m64, r64
  mov.is_64bit = 1;      // REX.W
  mov.use_modrm = 1;
  mov.mod = 1;           // 01 = Memory + Byte Displacement
  mov.rm = 5;            // Destination Base is RBP (ID 5)
  mov.use_disp = 1;      // We are using a displacement

  // Save RBX at -8: movq %rbx, -8(%rbp)
  mov.reg = 3;           // Source Register
  mov.displacement = -8;
  emit_instruction(code, pos, &mov);

  // Save R12 at -16: movq %r12, -16(%rbp)
  mov.reg = 12;           
  mov.displacement = -16;
  emit_instruction(code, pos, &mov);

  // Save R13 at -24: movq %r13, -24(%rbp)
  mov.reg = 13;
  mov.displacement = -24;
  emit_instruction(code, pos, &mov);

  // Save R14 at -32: movq %r14, -32(%rbp)
  mov.reg = 14;
  mov.displacement = -32;
  emit_instruction(code, pos, &mov);

  // Save R15 at -40: movq %r15, -40(%rbp)
  mov.reg = 15;
  mov.displacement = -40;
  emit_instruction(code, pos, &mov);
}

/**
 * Restores callee-saved registers values stored in the stack frame
 */
static void restore_callee_saved_registers(unsigned char code[], int* pos) {
  // Create a template for "MOV Memory[RBP + Offset] to Reg"
  Instruction mov = {0};
  mov.opcode = 0x8B;     // 0x8B = "Move FROM Memory TO Register" 
  mov.is_64bit = 1;      // REX.W
  mov.use_modrm = 1;
  mov.mod = 1;           // 01 = Memory + Byte Displacement
  mov.rm = 5;            // Source Base is RBP (ID 5)
  mov.use_disp = 1;      // We are using a displacement

  // Restore RBX (ID 3) from -8
  mov.reg = 3;           // Destination Register
  mov.displacement = -8;
  emit_instruction(code, pos, &mov);

  // Restore R12 (ID 12) from -16
  mov.reg = 12;          
  mov.displacement = -16;
  emit_instruction(code, pos, &mov);

  // Restore R13 (ID 13) from -24
  mov.reg = 13;
  mov.displacement = -24;
  emit_instruction(code, pos, &mov);

  // Restore R14 (ID 14) from -32
  mov.reg = 14;
  mov.displacement = -32;
  emit_instruction(code, pos, &mov);

  // Restore R15 (ID 15) from -40
  mov.reg = 15;
  mov.displacement = -40;
  emit_instruction(code, pos, &mov);
}

/**
 * Undoes the current stack frame: emits leave and ret
 */
static void emit_epilogue(unsigned char code[], int* pos) {
  // leave
  code[*pos] = 0xc9;
  (*pos)++;

  // ret
  code[*pos] = 0xc3;
  (*pos)++;
}

static void emit_return(unsigned char code[], int* pos, char retType,
                        int returnValue) {
  /**
   * local variable return (ret vX):
   * emits machine code for returning an SBas variable (v1 through v5)
   */
  if (retType == 'v') {
    RegInfo reg = new_get_local_var_reg(returnValue);
    if (reg.reg_code == -1) return;

    Instruction retVar = {0};
    retVar.opcode = 0x89;
    retVar.is_64bit = 1;

    retVar.use_modrm = 1;
    retVar.mod = 3;
    retVar.reg = reg.reg_code;  // from a general purpose reg
    retVar.rm = 0;  // to eax
    emit_instruction(code, pos, &retVar);
  }
  /**
   * constant literal return (ret $snum):
   * emits machine code for returning an immediate value (movl $imm32, %eax)
   */
  else if (retType == '$') {
    // optimization? instruction for putting immediates into eax (default, rd = 0)
    // https://www.felixcloutier.com/x86/mov

    code[*pos] = 0xb8;  // movl ..., %eax
    (*pos)++;

    emitIntegerInHex(code, pos, returnValue);
  }
  restore_callee_saved_registers(code, pos);
  emit_epilogue(code, pos);
}

/**
 * Emits machine code for a SBas attribution:
 * vX: <vX|pX|$num>
 */
static void emit_attribution(unsigned char code[], int* pos, int idxVar,
                             char varpcPrefix, int idxVarpc) {
  // att var to var
  if (varpcPrefix == 'v') {
    RegInfo src = new_get_local_var_reg(idxVarpc);
    RegInfo dst = new_get_local_var_reg(idxVar);
    if (src.reg_code == -1 || dst.reg_code == -1) return;

    Instruction movReg2Reg = {0};
    movReg2Reg.opcode = 0x89; // standard move
    movReg2Reg.is_64bit = 1;  // except for v1, v2-v5 are extended registers

    movReg2Reg.use_modrm = 1;
    movReg2Reg.mod = 3;
    movReg2Reg.reg = src.reg_code;
    movReg2Reg.rm = dst.reg_code;

    emit_instruction(code, pos, &movReg2Reg);
  }
  // att var param
  else if (varpcPrefix == 'p') {
    RegInfo src = get_param_reg(idxVarpc);
    RegInfo dst = new_get_local_var_reg(idxVar);
    if (src.reg_code == -1 || dst.reg_code == -1) return;

    Instruction movReg2Reg = {0};
    movReg2Reg.opcode = 0x89; // standard move

    movReg2Reg.use_modrm = 1;
    movReg2Reg.mod = 3;
    movReg2Reg.reg = src.reg_code;
    movReg2Reg.rm = dst.reg_code;
    emit_instruction(code, pos, &movReg2Reg);
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
static void emit_arithmetic_operation(unsigned char code[], int* pos,
                                      int idxVar, char varc1Prefix,
                                      int idxVarc1, char op, char varc2Prefix,
                                      int idxVarc2) {
  /**
   * For commutative operations, we swap the operands so we keep a single logic
   * path
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
    // 11 in binary: reg to reg operation
    int mode = 3;
    emit_modrm(code, pos, mode, src.reg_code, dst.reg_code);

  } else if (varc1Prefix == '$') {
    RegInfo dst = get_local_var_reg(idxVar);
    if (dst.reg_code == -1) return;

    emit_rex_byte(code, pos, 0, dst.rex);

    emit_mov_imm(code, pos, dst.reg_code, idxVarc1);
  } else {
    fprintf(stderr, "emit_arithmetic_operation: invalid varc1Prefix: %c\n",
            varc1Prefix);
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

    // Special case for multiplication: src and dst fields are swapped in REX
    // byte
    if (op == '*') {
      char tmp = src.rex;
      src.rex = dst.rex;
      dst.rex = tmp;
    }

    emit_rex_byte(code, pos, src.rex, dst.rex);

    switch (op) {
      case '+':
        code[(*pos)++] = 0x01;
        break;
      case '-':
        code[(*pos)++] = 0x29;
        break;
      case '*':
        code[(*pos)++] = 0x0F;
        code[(*pos)++] = 0xAF;
        break;
      default:
        fprintf(stderr, "emit_arithmetic_operation: invalid operation: %c\n",
                op);
        return;
    }

    // Special case for multiplication: reg and r/m are swapped in ModRM byte
    if (op == '*') {
      int temp = dst.reg_code;
      dst.reg_code = src.reg_code;
      src.reg_code = temp;
    }
    // 11 in binary: reg to reg operation
    int mode = 3;
    emit_modrm(code, pos, mode, src.reg_code, dst.reg_code);

  } else if (varc2Prefix == '$') {
    RegInfo dst = get_local_var_reg(idxVar);
    if (dst.reg_code == -1) return;

    if (dst.rex) {
      emit_rex_byte(code, pos, 1, dst.rex);
    }

    /**
     * Emit operations.
     * The ifs for checking if the number fits in a byte (-128 to 127) are due
     * to the possibility of emitting bytes for large values (imm32) and smaller
     * ones (imm8). Not sure if this is optimal though.
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
          emitIntegerInHex(code, pos, idxVarc2);
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
          emitIntegerInHex(code, pos, idxVarc2);
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
          emitIntegerInHex(code, pos, idxVarc2);
        }
        break;
      }
      default:
        fprintf(stderr, "emit_arithmetic_operation: invalid operation: %c\n",
                op);
        return;
    }
  }
}

/**
 * Writes first instruction of a SBas conditional jump (`iflez`):
 * cmpl $0, <variableRegister>
 * as well as the jle conditional jump opcode
 */
static void emit_cmp(unsigned char code[], int* pos, int varIndex) {
  RegInfo reg = get_local_var_reg(varIndex);
  if (reg.reg_code == -1) return;

  emit_rex_byte(code, pos, 0, reg.rex);

  // Emit `cmp $0, <reg>`
  code[(*pos)++] = 0x83;
  code[(*pos)++] = 0xF8 + reg.reg_code;
  code[(*pos)++] = 0x00;
}

/**
 * Emits `jle rel32`
 * which allows jumping to anywhere in code (+- 2GB)
 */
static void emit_jle(unsigned char code[], int* pos) {
  // jle rel32 must be followed by 4 bytes of offset
  code[(*pos)++] = 0x0F;
  code[(*pos)++] = 0x8E;

  // TODO: optimization?
  // short jumps such as those in tight loops only use 2 bytes:
  // 0x7E + 1 byte offset
  // https://www.felixcloutier.com/x86/jcc
}

/**
 * Returns the register where a local variable v1 through v5 is stored
 *
 * The `reg_code` field is a 3 bit value which represents the register code in
 * the ModRM byte The `rex` fields indicates if such register is extended (r12d
 * - r15d) i.e. in order to access it, a REX prefix byte is necessary.
 *
 * The function has no info regarding the usage of the register as source or
 * target. The caller knows whether the register is the source (`reg`) or the
 * target (`r/m`) of the operation in the ModRM byte, and, as such, will set the
 * REX.R or REX.B bits for source and target respectively.
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
      fprintf(stderr, "get_local_var_reg: invalid local variable index: v%d\n",
              idx);
      return (RegInfo){-1, -1};
  }
}

/**
 * Will be the new function to replace `get_local_var_reg`
 */
static RegInfo new_get_local_var_reg(int idx) {
  switch (idx) {
    case 1:
      // v1 -> ebx
      return (RegInfo){3, 0};
    case 2:
      // v2 -> r12d
      return (RegInfo){12, 1};
    case 3:
      // v3 -> r13d
      return (RegInfo){13, 1};
    case 4:
      // v4 -> r14d
      return (RegInfo){14, 1};
    case 5:
      // v5 -> r15d
      return (RegInfo){15, 1};
    default:
      fprintf(stderr, "get_local_var_reg: invalid local variable index: v%d\n",
              idx);
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
 * Emits, if necessary, the prefix REX byte.
 * This is mandatory for accessing extended registers like r12d through r15d
 *
 * If `src_rex` is true, the bit 2, called R, is set, extending the
 * `reg` field in ModRM (source register).
 *
 * If `dst_rex` is true, the bit 0, called B, is set, extending the
 * `r/m` field in ModRM (target register).
 *
 * If both are true, both bits are sets and the corresponding fields are
 * extended
 */
static void emit_rex_byte(unsigned char code[], int* pos, char src_rex,
                          char dst_rex) {
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

/**
 * Emits `mov` instruction
 */
static inline void emit_mov(unsigned char code[], int* pos) {
  code[*pos] = 0x89;
  (*pos)++;
}

/**
 * The ModRM byte tells the CPU what are the operands involved in an operation.
 * It is built like this:
 *
 * mod |  reg |  r/m
 *
 * bb  | bbb  |  bbb
 *
 * @param mode 2 bits telling the instruction is between which parts
 * @param source 3 bits mapping the source
 * @param dest 3 bits mapping the destination
 */
static void emit_modrm(unsigned char code[], int* pos, int mode, int source,
                       int dest) {
  code[(*pos)++] = (mode << 6) + (source << 3) + dest;
}

/**
 * Emits `mov` $imm, <reg>.
 * Does not use ModRM byte, rather:
 * 0xB8 + dst_reg_code followed by 4 bytes of imm
 */
static void emit_mov_imm(unsigned char code[], int* pos, int dst_reg_code,
                         int integer) {
  code[(*pos)++] = 0xB8 + dst_reg_code;
  emitIntegerInHex(code, pos, integer);
}

/**
 * A x86-64 instruction is made of:
 * - prefix (optional)
 * - opcode
 * - ModR/M (optional)
 * - payload (optional): immediates or memory offsets
 */
static void emit_instruction(unsigned char code[], int* pos,
                             Instruction* inst) {
  /** REX byte emission:
   * top 4 bits are fixed: 0100 (bin) / 4 (hex)
   * bottom 4 are the bits W, R, X, and B
   * W extends instruction to 64 bits
   * R extends source register
   * B extends destination register
   */
  unsigned char rex = 0x40;
  char needs_rex = 0;

  // REX.W: Promotion to 64-bit width
  if (inst->is_64bit) {
    rex |= 0x08;
    needs_rex = 1;
  }

  // REX.R: Extension for the 'reg' field (source)
  // used for registers of id 8-15
  if (inst->use_modrm && inst->reg > 7) {
    rex |= 0x04;
    needs_rex = 1;
  }

  // REX.B: Extension for the 'r/m' field (destination)
  // used for registers of id 8-15
  if (inst->use_modrm && inst->rm > 7) {
    rex |= 0x01;
    needs_rex = 1;
  }

  if (needs_rex) {
    code[(*pos)++] = rex;
  }

  // Handle 2-byte opcodes (like 0x0FAF for IMUL)
  if (inst->opcode > 0xFF) {
    code[(*pos)++] = (inst->opcode >> 8) & 0xFF;  // High byte
    code[(*pos)++] = inst->opcode & 0xFF;         // Low byte
  } else {
    code[(*pos)++] = (unsigned char)inst->opcode;
  }

  if (inst->use_modrm) {
    unsigned char modrm = 0;

    // Shift mode to bits 7-6
    modrm |= (inst->mod << 6);

    // Shift reg to bits 5-3 (Mask with 7 to strip high bit handled by REX)
    modrm |= ((inst->reg & 7) << 3);

    // Shift rm to bits 2-0 (Mask with 7)
    modrm |= (inst->rm & 7);

    code[(*pos)++] = modrm;
  }

  // ---------------------------------------------------------
  // 4. PAYLOAD PHASE (Displacement & Immediate)
  // ---------------------------------------------------------
  if (inst->use_disp) {
    // For now, assuming 8-bit displacement (signed char) as used in your stack
    code[(*pos)++] = (unsigned char)(inst->displacement & 0xFF);
  }

  if (inst->use_imm) {
    if (inst->imm_size == 4) {
      // Use your existing helper for 32-bit numbers
      emitIntegerInHex(code, pos, inst->immediate);
    } else {
      // 8-bit immediate
      code[(*pos)++] = (unsigned char)(inst->immediate & 0xFF);
    }
  }
}