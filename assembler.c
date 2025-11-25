#include "assembler.h"

#include <stdio.h>

#include "config.h"
#include "utils.h"

#define BUFFER_SIZE 128  // length of a parsed line and of an error message

static void emit_instruction(unsigned char code[], int* pos, Instruction* inst);
static void emit_prologue(unsigned char code[], int* pos);
static void save_callee_saved_registers(unsigned char code[], int* pos);
static void emit_return(unsigned char code[], int* pos, char retType, int returnValue);
static void emit_attribution(unsigned char code[], int* pos, int idxVar, char varpcPrefix, int idxVarpc);
static void emit_arithmetic_operation(unsigned char code[], int* pos, int idxVar, char varc1Prefix, int idxVarc1, char op, char varc2Prefix, int idxVarc2);
static void emit_cmp(unsigned char code[], int* pos, int varIndex);
static void emit_jle(unsigned char code[], int* pos);
static void restore_callee_saved_registers(unsigned char code[], int* pos);
static void emit_epilogue(unsigned char code[], int* pos);
static int get_hardware_reg_index(char type, int idx);

typedef enum {
  OP_SAVE_BASE_PTR_IN_STACK_FRAME = 0x55,               // pushq %rbp
  OP_MOV_FROM_REG_TO_RM = 0x89,                         // move r32/64 to r/m 32/64
  OP_MOV_FROM_RM_TO_REG = 0x8B,                         // move r/m 32/64 to r32/64
  OP_MOV_FROM_IMM_TO_REG = 0xB8,                        // move imm32/64 to r32/64 (requires rd embedded in opcode)
  OP_IMM8_ARITHM_OP = 0x83,                             // arithmetic operation with byte immediate
  OP_ADD_REG_TO_RM = 0x01,                              // add r32/64 to r/m 32/64
  OP_SUB_REG_FROM_RM = 0x29,                            // subtract r32/64 from r/m 32/64
  OP_IMUL_REG_BY_RM_STORE_IN_REG = (0x0F << 8) | 0xAF,  // multiply r/m 32/64 by r32/64 and store in r32/64 (r32/64 := r/m 32/64 * r32/64 )
  OP_IMUL_RM_BY_BYTE_STORE_IN_REG = 0x6B,               // multiply r/m 32/64 by imm8 and store in r32/64
  OP_IMUL_RM_BY_INT_STORE_IN_REG = 0x69,                // multiply r/m 32/64 by imm32 and store in r32/64
  OP_LEAVE = 0xc9,
  OP_RET = 0xc3,
} Opcode;

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
char sbasAssemble(unsigned char* code, FILE* f, LineTable* lt, RelocationTable* rt, int* relocCount) {
  unsigned line = 1;                       // count in the SBas file
  char lineBuffer[BUFFER_SIZE] = {0};      // a line in the SBas file
  char errorMsgBuffer[BUFFER_SIZE] = {0};  // a compilation error message
  int pos = 0;                             // byte position in the buffer
  char retFound = 0;                       // every SBas function MUST return

  emit_prologue(code, &pos);
  save_callee_saved_registers(code, &pos);

  while (fgets(lineBuffer, sizeof(lineBuffer), f)) {
    trimLeadingSpaces(lineBuffer);

    if (lineBuffer[0] == ' ' || lineBuffer[0] == '\n' || lineBuffer[0] == '\0' || lineBuffer[0] == '/') {
      line++;
      continue;
    }

    if (line > MAX_LINES) {
      fprintf(stderr, "sbasCompile: the provided SBas file exceeds MAX_LINES (%d)!\n", MAX_LINES);
      return -1;
    }

    lt[line].line = line;
    lt[line].offset = pos;

    switch (lineBuffer[0]) {
      case 'r': { /* return */
        int varc;
        char retType;  // 'v' for variable return, '$' for immediate value return

        if (sscanf(lineBuffer, "ret %c%d", &retType, &varc) != 2 || (retType != 'v' && retType != '$')) {
          compilationError("sbasCompile: invalid 'ret' command: expected 'ret <var|$int>", line);
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
          compilationError("sbasCompile: invalid command: expected attribution (vX: varpc) or arithmetic operation (vX = varc op varc)", line);
          return -1;
        }

        if (idxVar < 1 || idxVar > 5) {
          snprintf(errorMsgBuffer, BUFFER_SIZE, "sbasCompile: invalid local variable index %d. Only v1 through v5 are allowed.", idxVar);
          compilationError(errorMsgBuffer, line);
          return -1;
        }

        if (operator != ':' && operator != '=') {
          snprintf(errorMsgBuffer, BUFFER_SIZE, "sbasCompile: invalid operator %c. Only attribution (:) and arithmetic operation (=) are supported.", operator);
          compilationError(errorMsgBuffer, line);
          return -1;
        }

        // attribution
        if (operator == ':') {
          char varpcPrefix;
          int idxVarpc;
          if (sscanf(lineBuffer, "v%d : %c%d", &idxVar, &varpcPrefix, &idxVarpc) != 3) {
            compilationError("sbasCompile: invalid attribution: expected 'vX: <vX|pX|$num>'", line);
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
          char remaining[BUFFER_SIZE] = {0};  // used in scanset to detect extra operands/operators

          if (sscanf(lineBuffer, "v%d = %c%d %c %c%d %127[^\n]", &idxVar, &varc1Prefix, &idxVarc1, &op, &varc2Prefix, &idxVarc2, remaining) != 6) {
            compilationError("sbasCompile: invalid arithmetic operation: expected 'vX = <vX|$num> op <vX|$num>'", line);
            return -1;
          }

          if (op != '+' && op != '-' && op != '*') {
            snprintf(errorMsgBuffer, BUFFER_SIZE, "sbasCompile: invalid arithmetic operation %c. Only addition (+), subtraction (-), and multiplication (*) allowed.", op);
            compilationError(errorMsgBuffer, line);
            return -1;
          }

          emit_arithmetic_operation(code, &pos, idxVar, varc1Prefix, idxVarc1, op, varc2Prefix, idxVarc2);
        }

        break;
      }
      case 'i': { /* conditional jump */
        int varIndex;
        unsigned lineTarget;

        if (sscanf(lineBuffer, "iflez v%d %u", &varIndex, &lineTarget) != 2) {
          compilationError("sbasCompile: invalid 'iflez' command: expected 'iflez vX line'", line);
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
    fprintf(stderr, "sbasCompile: SBas function doesn't include 'ret'. Aborting!\n");
    return -1;
  }

#ifdef DEBUG
  printf("sbasCompile: processed %d lines, writing %d bytes in buffer\n", line - 1, pos);
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
  code[*pos] = OP_SAVE_BASE_PTR_IN_STACK_FRAME;
  (*pos)++;

  // movq %rsp, %rbp
  Instruction initStackFrame = {0};
  initStackFrame.opcode = OP_MOV_FROM_REG_TO_RM;
  initStackFrame.is_64bit = 1;

  initStackFrame.use_modrm = 1;
  initStackFrame.mod = 3;  // between registers
  initStackFrame.reg = 4;  // from rsp
  initStackFrame.rm = 5;   // to rbp

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
  sub.opcode = OP_IMM8_ARITHM_OP;
  sub.is_64bit = 1;
  sub.use_modrm = 1;
  sub.mod = 3;  // 11 = Register Direct Mode
  sub.reg = 5;  // 5 = Opcode Extension for SUB
  sub.rm = 4;   // 4 = RSP Register ID
  sub.use_imm = 1;
  sub.imm_size = 1;    // 1 byte immediate
  sub.immediate = 48;  // Value 48
  emit_instruction(code, pos, &sub);

  // 2. Save Registers to Stack
  // Create a template for "MOV Reg to Memory[RBP + Offset]"
  Instruction mov = {0};
  mov.opcode = OP_MOV_FROM_REG_TO_RM;
  mov.is_64bit = 1;  // REX.W
  mov.use_modrm = 1;
  mov.mod = 1;       // 01 = Memory + Byte Displacement
  mov.rm = 5;        // Destination Base is RBP (ID 5)
  mov.use_disp = 1;  // We are using a displacement

  // Save RBX at -8: movq %rbx, -8(%rbp)
  mov.reg = 3;  // Source Register
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
  mov.opcode = OP_MOV_FROM_RM_TO_REG;
  mov.is_64bit = 1;  // REX.W
  mov.use_modrm = 1;
  mov.mod = 1;       // 01 = Memory + Byte Displacement
  mov.rm = 5;        // Source Base is RBP (ID 5)
  mov.use_disp = 1;  // We are using a displacement

  // Restore RBX (ID 3) from -8
  mov.reg = 3;  // Destination Register
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
  code[*pos] = OP_LEAVE;
  (*pos)++;

  // ret
  code[*pos] = OP_RET;
  (*pos)++;
}

static void emit_return(unsigned char code[], int* pos, char retType, int returnValue) {
  Instruction retVar = {0};
  /**
   * local variable return (ret vX):
   * emits machine code for returning an SBas variable (v1 through v5)
   */
  if (retType == 'v') {
    int regCode = get_hardware_reg_index('v', returnValue);
    if (regCode == -1) return;

    retVar.opcode = OP_MOV_FROM_REG_TO_RM;
    retVar.is_64bit = 1;

    retVar.use_modrm = 1;
    retVar.mod = 3;
    retVar.reg = regCode;  // from a general purpose reg
    retVar.rm = 0;         // to eax
    emit_instruction(code, pos, &retVar);
  }
  /**
   * constant literal return (ret $snum):
   * emits machine code for returning an immediate value (movl $imm32, %eax)
   */
  else if (retType == '$') {
    // mov(l) $imm32, %eax
    retVar.opcode = OP_MOV_FROM_IMM_TO_REG;
    retVar.is_small_ret = 1;
    retVar.small_ret_rd = 0;  // being explicit: eax's rd = 0
    retVar.use_imm = 1;
    retVar.immediate = returnValue;
    retVar.imm_size = 4;
    emit_instruction(code, pos, &retVar);
  }
  restore_callee_saved_registers(code, pos);
  emit_epilogue(code, pos);
}

/**
 * Emits machine code for a SBas attribution:
 * vX: <vX|pX|$num>
 */
static void emit_attribution(unsigned char code[], int* pos, int idxVar, char varpcPrefix, int idxVarpc) {
  Instruction movReg2Reg = {0};
  // att var to var
  if (varpcPrefix == 'v') {
    int srcRegCode = get_hardware_reg_index('v', idxVarpc);
    int dstRegCode = get_hardware_reg_index('v', idxVar);
    if (srcRegCode == -1 || dstRegCode == -1) return;

    movReg2Reg.opcode = OP_MOV_FROM_REG_TO_RM;
    movReg2Reg.is_64bit = 1;  // except for v1, v2-v5 are extended registers

    movReg2Reg.use_modrm = 1;
    movReg2Reg.mod = 3;
    movReg2Reg.reg = srcRegCode;
    movReg2Reg.rm = dstRegCode;

    emit_instruction(code, pos, &movReg2Reg);
  }
  // att var param
  else if (varpcPrefix == 'p') {
    int srcRegCode = get_hardware_reg_index('p', idxVarpc);
    int dstRegCode = get_hardware_reg_index('v', idxVar);
    if (srcRegCode == -1 || dstRegCode == -1) return;

    movReg2Reg.opcode = OP_MOV_FROM_REG_TO_RM;

    movReg2Reg.use_modrm = 1;
    movReg2Reg.mod = 3;
    movReg2Reg.reg = srcRegCode;
    movReg2Reg.rm = dstRegCode;
    emit_instruction(code, pos, &movReg2Reg);
  }
  // att var imm
  else if (varpcPrefix == '$') {
    int dstRegCode = get_hardware_reg_index('v', idxVar);

    movReg2Reg.opcode = OP_MOV_FROM_IMM_TO_REG;

    movReg2Reg.is_small_ret = 1;
    movReg2Reg.small_ret_rd = dstRegCode;

    movReg2Reg.use_imm = 1;
    movReg2Reg.imm_size = 4;
    movReg2Reg.immediate = idxVarpc;
    emit_instruction(code, pos, &movReg2Reg);
  }
}

/**
 * Emit machine code for a SBas arithmetic operation:
 * vX = <vX | $num> op <vX | $num>
 */
static void emit_arithmetic_operation(unsigned char code[], int* pos, int idxVar, char varc1Prefix, int idxVarc1, char op, char varc2Prefix, int idxVarc2) {
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

  Instruction movReg2Reg = {0};
  /**
   * Emits first instruction of an arithmetic operation:
   * mov <leftOperand>, <attributedVar>
   */
  if (varc1Prefix == 'v') {
    int srcRegCode = get_hardware_reg_index('v', idxVarc1);
    int dstRegCode = get_hardware_reg_index('v', idxVar);
    if (srcRegCode == -1 || dstRegCode == -1) return;

    movReg2Reg.opcode = OP_MOV_FROM_REG_TO_RM;
    movReg2Reg.use_modrm = 1;
    movReg2Reg.mod = 3;
    movReg2Reg.reg = srcRegCode;
    movReg2Reg.rm = dstRegCode;

    emit_instruction(code, pos, &movReg2Reg);
  } else if (varc1Prefix == '$') {
    int dstRegCode = get_hardware_reg_index('v', idxVar);
    movReg2Reg.opcode = OP_MOV_FROM_IMM_TO_REG;

    movReg2Reg.is_small_ret = 1;
    movReg2Reg.small_ret_rd = dstRegCode;

    movReg2Reg.use_imm = 1;
    movReg2Reg.imm_size = 4;
    movReg2Reg.immediate = idxVarc1;
    emit_instruction(code, pos, &movReg2Reg);
  } else {
    fprintf(stderr, "emit_arithmetic_operation: invalid varc1Prefix: %c\n", varc1Prefix);
    return;
  }

  Instruction arithmOp = {0};
  /**
   * Emits second instruction of an arithmetic operation:
   * <op> <rightOperand>, <attributedVar>
   */
  if (varc2Prefix == 'v') {
    int srcRegCode = get_hardware_reg_index('v', idxVarc2);
    int dstRegCode = get_hardware_reg_index('v', idxVar);
    if (srcRegCode == -1 || dstRegCode == -1) return;

    switch (op) {
      case '+':
        arithmOp.opcode = OP_ADD_REG_TO_RM;
        break;
      case '-':
        arithmOp.opcode = OP_SUB_REG_FROM_RM;
        break;
      case '*':
        arithmOp.opcode = OP_IMUL_REG_BY_RM_STORE_IN_REG;
        break;
      default:
        fprintf(stderr, "emit_arithmetic_operation: invalid operation: %c\n", op);
        return;
    }

    // Special case for multiplication: reg and r/m are swapped in ModRM byte
    if (op == '*') {
      int temp = dstRegCode;
      dstRegCode = srcRegCode;
      srcRegCode = temp;
    }

    arithmOp.use_modrm = 1;
    arithmOp.mod = 3;
    arithmOp.reg = srcRegCode;
    arithmOp.rm = dstRegCode;
    emit_instruction(code, pos, &arithmOp);
  } else if (varc2Prefix == '$') {
    int dstRegCode = get_hardware_reg_index('v', idxVar);
    if (dstRegCode == -1) return;

    // TODO: 0x83 is used for all imm8 ops and 0x81 for imm32, difference is reg field
    // TODO: in small multiplication, code[(*pos)++] = dstRegCode * 9; is equivalent to ((dstRegCode << 3) | dstRegCode)

    /**
     * Emit operations.
     * The ifs for checking if the number fits in a byte (-128 to 127) are due
     * to the possibility of emitting bytes for large values (imm32) and smaller
     * ones (imm8). Not sure if this is optimal though.
     */
    switch (op) {
      case '+': {
        if (idxVarc2 >= -128 && idxVarc2 <= 127) {
          arithmOp.opcode = OP_IMM8_ARITHM_OP;  // add(b) imm8, r/m32
          arithmOp.isArithmOp = 1;
          arithmOp.use_modrm = 1;
          arithmOp.mod = 3;
          arithmOp.rm = dstRegCode;
          arithmOp.use_imm = 1;
          arithmOp.imm_size = 1;  // 8 bits
          arithmOp.immediate = idxVarc2;

          emit_instruction(code, pos, &arithmOp);
        } else {
          arithmOp.opcode = 0x81;  // add(l) imm32, r/m32
          arithmOp.use_imm = 1;
          arithmOp.imm_size = 4;  // 32 bits
          arithmOp.immediate = idxVarc2;
          arithmOp.isArithmOp = 1;
          arithmOp.use_modrm = 1;
          arithmOp.mod = 3;
          arithmOp.rm = dstRegCode;
          emit_instruction(code, pos, &arithmOp);
        }
        break;
      }
      case '-': {
        if (idxVarc2 >= -128 && idxVarc2 <= 127) {
          arithmOp.opcode = OP_IMM8_ARITHM_OP;  // sub(b) imm8, r/m32
          arithmOp.isArithmOp = 1;
          arithmOp.use_modrm = 1;
          arithmOp.mod = 3;
          arithmOp.rm = dstRegCode;
          arithmOp.reg = 5;  // 101 in reg, indicating subtraction
          arithmOp.use_imm = 1;
          arithmOp.imm_size = 1;  // 8 bits
          arithmOp.immediate = idxVarc2;

          emit_instruction(code, pos, &arithmOp);
        } else {
          arithmOp.opcode = 0x81;  // sub(l) imm32, r/m32
          arithmOp.use_imm = 1;
          arithmOp.imm_size = 4;  // 32 bits
          arithmOp.immediate = idxVarc2;
          arithmOp.isArithmOp = 1;
          arithmOp.use_modrm = 1;
          arithmOp.mod = 3;
          arithmOp.rm = dstRegCode;
          arithmOp.reg = 5;  // 101 in reg, indicating subtraction
          emit_instruction(code, pos, &arithmOp);
        }
        break;
      }
      case '*': {
        if (idxVarc2 >= -128 && idxVarc2 <= 127) {
          arithmOp.opcode = OP_IMUL_RM_BY_BYTE_STORE_IN_REG;  // imul(b) imm8, r/m32
          arithmOp.use_imm = 1;
          arithmOp.imm_size = 1;  // 8 bits
          arithmOp.immediate = idxVarc2;
          arithmOp.isArithmOp = 1;
          arithmOp.use_modrm = 1;
          arithmOp.mod = 3;
          // src and dst register are the same
          arithmOp.rm = dstRegCode;
          arithmOp.reg = dstRegCode;
          emit_instruction(code, pos, &arithmOp);
        } else {
          arithmOp.opcode = OP_IMUL_RM_BY_INT_STORE_IN_REG;  // imul(l) imm32, r/m32
          arithmOp.use_imm = 1;
          arithmOp.imm_size = 4;  // 32 bits
          arithmOp.immediate = idxVarc2;
          arithmOp.isArithmOp = 1;
          arithmOp.use_modrm = 1;
          arithmOp.mod = 3;
          // src and dst register are the same
          arithmOp.rm = dstRegCode;
          arithmOp.reg = dstRegCode;
          emit_instruction(code, pos, &arithmOp);
        }
        break;
      }
      default: {
        fprintf(stderr, "emit_arithmetic_operation: invalid operation: %c\n", op);
        return;
      }
    }
  }
}

/**
 * Writes first instruction of a SBas conditional jump (`iflez`):
 * cmpl $0, <variableRegister>
 * as well as the jle conditional jump opcode
 */
static void emit_cmp(unsigned char code[], int* pos, int varIndex) {
  int regCode = get_hardware_reg_index('v', varIndex);
  if (regCode == -1) return;

  Instruction cmp = {0};
  cmp.opcode = OP_IMM8_ARITHM_OP;
  cmp.use_modrm = 1;
  cmp.mod = 3;  // 11
  cmp.reg = 7;  // 111 (cmp)
  cmp.rm = regCode;
  cmp.isCmp = 1;

  emit_instruction(code, pos, &cmp);
}

/**
 * Emits `jle rel32`
 * which allows jumping to anywhere in code (+- 2GB)
 */
static void emit_jle(unsigned char code[], int* pos) {
  // jle rel32 must be followed by 4 bytes of offset
  code[(*pos)++] = 0x0F;
  code[(*pos)++] = 0x8E;

  // TODO: optimization? short jumps such as those in tight loops only use 2 bytes: 0x7E + 1 byte offset
  // https://www.felixcloutier.com/x86/jcc
}

/**
 * Unified Register Mapper.
 * Returns the Full Hardware Index (0-15).
 * * Supports:
 * - Locals ('v'): v1(RBX), v2(R12), v3(R13), v4(R14), v5(R15)
 * - Params ('p'): p1(EDI), p2(ESI), p3(EDX)
 */
static int get_hardware_reg_index(char type, int idx) {
  if (type == 'v') {
    switch (idx) {
      case 1:
        return 3;  // RBX
      case 2:
        return 12;  // R12
      case 3:
        return 13;  // R13
      case 4:
        return 14;  // R14
      case 5:
        return 15;  // R15
      default:
        return -1;
    }
  } else if (type == 'p') {
    switch (idx) {
      case 1:
        return 7;  // RDI (System V ABI 1st arg)
      case 2:
        return 6;  // RSI (System V ABI 2nd arg)
      case 3:
        return 2;  // RDX (System V ABI 3rd arg)
      default:
        return -1;
    }
  }
  return -1;
}

/**
 * A x86-64 instruction is made of:
 * - prefix (optional)
 * - opcode
 * - ModR/M (optional)
 * - payload (optional): immediates or memory offsets
 */
static void emit_instruction(unsigned char code[], int* pos, Instruction* inst) {
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

  if (inst->is_small_ret) {
    // Case: Opcode embedding (e.g. 0xB8 + reg)
    // We only need REX.B if the register index is 8-15 (r8-r15)
    if (inst->small_ret_rd > 7) {
      rex |= 0x01;  // REX.B
      needs_rex = 1;
    }
  } else if (inst->isCmp) {
    if (inst->rm > 7) {
      rex |= 0x01;
      needs_rex = 1;
    }
  } else {
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
  }

  if (needs_rex) {
    code[(*pos)++] = rex;
  }

  // Handle 2-byte opcodes (like imul's 0x0FAF)
  if (inst->opcode > 0xFF) {
    code[(*pos)++] = (inst->opcode >> 8) & 0xFF;  // High byte
    code[(*pos)++] = inst->opcode & 0xFF;         // Low byte
  } else {
    unsigned int combinedOpcode = inst->opcode;

    if (inst->is_small_ret) {
      combinedOpcode += (inst->small_ret_rd & 7);
    }
    code[(*pos)++] = (unsigned char)combinedOpcode;
  }

  if (inst->use_modrm) {
    unsigned char modrm = 0;

    modrm |= (inst->mod << 6);
    modrm |= ((inst->reg & 7) << 3);
    modrm |= (inst->rm & 7);

    code[(*pos)++] = modrm;
  }

  // Memory offsets handling
  if (inst->use_disp) {
    // For now, assuming 8-bit displacement (signed char) as used in your stack
    code[(*pos)++] = (unsigned char)(inst->displacement & 0xFF);
  }

  // Immediate values handling
  if (inst->use_imm) {
    if (inst->imm_size == 4) {
      // Use your existing helper for 32-bit numbers
      emitIntegerInHex(code, pos, inst->immediate);
    } else {
      // 8-bit immediate
      code[(*pos)++] = (unsigned char)(inst->immediate & 0xFF);
    }
  }

  // For now, support only iflez (compares against zero, so hardcode it)
  if (inst->isCmp) {
    code[(*pos)++] = 0x00;
  }
}