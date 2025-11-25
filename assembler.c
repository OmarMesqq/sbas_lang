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
  OP_MOV_REG_TO_RM = 0x89,                              // move r32/64 to r/m 32/64
  OP_MOV_RM_TO_REG = 0x8B,                              // move r/m 32/64 to r32/64
  OP_MOV_IMM_TO_RD = 0xB8,                              // move imm32/64 to r32/64 (requires register id `rd` embedded in opcode)
  OP_IMM8_ARITHM_OP = 0x83,                             // arithmetic operation with byte immediate
  OP_IMM32_ARITHM_OP = 0x81,                            // arithmetic operation with int immediate
  OP_ADD_REG_TO_RM = 0x01,                              // add r32/64 to r/m 32/64
  OP_SUB_REG_FROM_RM = 0x29,                            // subtract r32/64 from r/m 32/64
  OP_IMUL_REG_BY_RM_STORE_IN_REG = (0x0F << 8) | 0xAF,  // multiply r/m 32/64 by r32/64 and store in r32/64 (r32/64 := r/m 32/64 * r32/64 )
  OP_IMUL_RM_BY_BYTE_STORE_IN_REG = 0x6B,               // multiply r/m 32/64 by imm8 and store in r32/64
  OP_IMUL_RM_BY_INT_STORE_IN_REG = 0x69,                // multiply r/m 32/64 by imm32 and store in r32/64
  OP_LEAVE = 0xc9,
  OP_RET = 0xc3,
} Opcode;

/**
 * Defines the two addressing modes used by the SBas compiler
 * that go into the `mod` field of the ModRM byte i.e.
 * bits 7 and 6 of the latter
 */
typedef enum {
  // (11) Both operands are registers. Used for variables
  MOD_REGISTER_DIRECT = 3,

  // (01) Memory access: (register + signed byte). Used for stack frame offsets
  MOD_REG_PLUS_DISP8 = 1,
} Mod;

/**
 * Single source of truth for getting x86-64's (messy) register IDs
 */
typedef enum {
  REG_RAX = 0,   // caller-saved: return register
  REG_RCX = 1,   // caller-saved: 4th argument in System V ABI
  REG_RDX = 2,   // caller-saved: 3rd argument in System V ABI
  REG_RBX = 3,   // callee-saved: general purpose
  REG_RSP = 4,   // stack pointer
  REG_RBP = 5,   // frame or base pointer
  REG_RSI = 6,   // caller-saved: 2nd argument in System V ABI
  REG_RDI = 7,   // caller-saved: 1st argument in System V ABI
  REG_R8 = 8,    // caller-saved: 5th argument in System V ABI
  REG_R9 = 9,    // caller-saved: 6th argument in System V ABI
  REG_R10 = 10,  // caller-saved: general purpose
  REG_R11 = 11,  // caller-saved: general purpose
  REG_R12 = 12,  // callee-saved: general purpose
  REG_R13 = 13,  // callee-saved: general purpose
  REG_R14 = 14,  // callee-saved: general purpose
  REG_R15 = 15   // callee-saved: general purpose
} Register;

/**
 * Opcodes that perform arithmetic operations on immediates form a sort
 * of "base" opcode group that specializes by tuning bits in the `reg` field
 * of ModRM
 */
typedef enum {
  EXT_ADD = 0,
  EXT_SUB = 5,  // 101
  EXT_CMP = 7   // 111
} OpcodeExtension;

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
  Instruction initializeStackPtr = {0};
  initializeStackPtr.opcode = OP_MOV_REG_TO_RM;
  initializeStackPtr.is_64bit = 1;

  initializeStackPtr.use_modrm = 1;
  initializeStackPtr.mod = MOD_REGISTER_DIRECT;
  initializeStackPtr.reg = REG_RSP;
  initializeStackPtr.rm = REG_RBP;

  emit_instruction(code, pos, &initializeStackPtr);
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
  // subq $48, %rsp
  Instruction decrementStackPtr = {0};
  decrementStackPtr.opcode = OP_IMM8_ARITHM_OP;
  decrementStackPtr.is_64bit = 1;
  decrementStackPtr.use_modrm = 1;
  decrementStackPtr.mod = MOD_REGISTER_DIRECT;
  decrementStackPtr.reg = EXT_SUB;
  decrementStackPtr.rm = REG_RSP;
  decrementStackPtr.use_imm = 1;
  decrementStackPtr.imm_size = 1;
  decrementStackPtr.immediate = 48;
  emit_instruction(code, pos, &decrementStackPtr);

  Instruction movRegToStack = {0};
  movRegToStack.opcode = OP_MOV_REG_TO_RM;
  movRegToStack.is_64bit = 1;
  movRegToStack.use_disp = 1;
  movRegToStack.use_modrm = 1;
  movRegToStack.mod = MOD_REG_PLUS_DISP8;
  movRegToStack.rm = REG_RBP;

  // movq %rbx, -8(%rbp)
  movRegToStack.reg = REG_RBX;
  movRegToStack.displacement = -8;
  emit_instruction(code, pos, &movRegToStack);

  // movq %r12, -16(%rbp)
  movRegToStack.reg = REG_R12;
  movRegToStack.displacement = -16;
  emit_instruction(code, pos, &movRegToStack);

  // movq %r13, -24(%rbp)
  movRegToStack.reg = REG_R13;
  movRegToStack.displacement = -24;
  emit_instruction(code, pos, &movRegToStack);

  // movq %r14, -32(%rbp)
  movRegToStack.reg = REG_R14;
  movRegToStack.displacement = -32;
  emit_instruction(code, pos, &movRegToStack);

  // movq %r15, -40(%rbp)
  movRegToStack.reg = REG_R15;
  movRegToStack.displacement = -40;
  emit_instruction(code, pos, &movRegToStack);
}

/**
 * Restores callee-saved registers values stored in the stack frame
 */
static void restore_callee_saved_registers(unsigned char code[], int* pos) {
  Instruction movFromStackToReg = {0};
  movFromStackToReg.opcode = OP_MOV_RM_TO_REG;
  movFromStackToReg.is_64bit = 1;
  movFromStackToReg.use_disp = 1;
  movFromStackToReg.use_modrm = 1;
  movFromStackToReg.mod = MOD_REG_PLUS_DISP8;
  movFromStackToReg.rm = REG_RBP;

  // movq -8(%rbp), %rbx
  movFromStackToReg.reg = REG_RBX;
  movFromStackToReg.displacement = -8;
  emit_instruction(code, pos, &movFromStackToReg);

  // movq -16(%rbp), %r12
  movFromStackToReg.reg = REG_R12;
  movFromStackToReg.displacement = -16;
  emit_instruction(code, pos, &movFromStackToReg);

  // movq -24(%rbp), %r13
  movFromStackToReg.reg = REG_R13;
  movFromStackToReg.displacement = -24;
  emit_instruction(code, pos, &movFromStackToReg);

  // movq -32(%rbp), %r14
  movFromStackToReg.reg = REG_R14;
  movFromStackToReg.displacement = -32;
  emit_instruction(code, pos, &movFromStackToReg);

  // movq -40(%rbp), %r15
  movFromStackToReg.reg = REG_R15;
  movFromStackToReg.displacement = -40;
  emit_instruction(code, pos, &movFromStackToReg);
}

/**
 * Undoes the current stack frame: emits leave and ret
 */
static void emit_epilogue(unsigned char code[], int* pos) {
  code[*pos] = OP_LEAVE;
  (*pos)++;

  code[*pos] = OP_RET;
  (*pos)++;
}

static void emit_return(unsigned char code[], int* pos, char retType, int returnValue) {
  Instruction _return = {0};

  // local variable return (ret vX)
  if (retType == 'v') {
    int regCode = get_hardware_reg_index(retType, returnValue);
    if (regCode == -1) return;

    _return.opcode = OP_MOV_REG_TO_RM;

    _return.use_modrm = 1;
    _return.mod = MOD_REGISTER_DIRECT;
    _return.reg = regCode;
    _return.rm = REG_RAX;
  }
  // constant literal return (ret $snum)
  else if (retType == '$') {
    _return.opcode = OP_MOV_IMM_TO_RD;

    _return.is_imm_mov = 1;
    _return.imm_mov_rd = REG_RAX;

    _return.use_imm = 1;
    _return.immediate = returnValue;
    _return.imm_size = 4;
  } else {
    fprintf(stderr, "emit_return: invalid return type: %c\n", retType);
    return;
  }
  emit_instruction(code, pos, &_return);
  restore_callee_saved_registers(code, pos);
  emit_epilogue(code, pos);
}

/**
 * Emits machine code for a SBas attribution:
 * vX: <vX|pX|$num>
 */
static void emit_attribution(unsigned char code[], int* pos, int idxVar, char varpcPrefix, int idxVarpc) {
  Instruction attribution = {0};

  // target of an attribution is always a register
  int dstRegCode = get_hardware_reg_index('v', idxVar);
  if (dstRegCode == -1) return;

  // var to var attribution (vX : vY) and param to var attribution (vX : pY)
  if (varpcPrefix == 'v' || varpcPrefix == 'p') {
    int srcRegCode = get_hardware_reg_index(varpcPrefix, idxVarpc);
    attribution.opcode = OP_MOV_REG_TO_RM;

    attribution.use_modrm = 1;
    attribution.mod = MOD_REGISTER_DIRECT;
    attribution.reg = srcRegCode;
    attribution.rm = dstRegCode;
  }
  // imm to var attribution (vX: $snum)
  else if (varpcPrefix == '$') {
    attribution.opcode = OP_MOV_IMM_TO_RD;

    attribution.is_imm_mov = 1;
    attribution.imm_mov_rd = dstRegCode;

    attribution.use_imm = 1;
    attribution.imm_size = 4;
    attribution.immediate = idxVarpc;
  } else {
    fprintf(stderr, "emit_attribution: invalid attribution target: %c\n", varpcPrefix);
    return;
  }
  emit_instruction(code, pos, &attribution);
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
   * First instruction of an arithmetic operation:
   * mov <leftOperand>, <attributedVar>
   */
  Instruction mov = {0};

  // target of an arithmetic operation is always a register
  int dstRegCode = get_hardware_reg_index('v', idxVar);
  if (dstRegCode == -1) return;

  if (varc1Prefix == 'v') {
    int srcRegCode = get_hardware_reg_index(varc1Prefix, idxVarc1);
    if (srcRegCode == -1) return;

    mov.opcode = OP_MOV_REG_TO_RM;

    mov.use_modrm = 1;
    mov.mod = MOD_REGISTER_DIRECT;
    mov.reg = srcRegCode;
    mov.rm = dstRegCode;
  } else if (varc1Prefix == '$') {
    mov.opcode = OP_MOV_IMM_TO_RD;

    mov.is_imm_mov = 1;
    mov.imm_mov_rd = dstRegCode;

    mov.use_imm = 1;
    mov.imm_size = 4;
    mov.immediate = idxVarc1;
  } else {
    fprintf(stderr, "emit_arithmetic_operation: invalid varc1Prefix: %c\n", varc1Prefix);
    return;
  }
  emit_instruction(code, pos, &mov);

  /**
   * Second instruction of an arithmetic operation:
   * <operation> <rightOperand>, <attributedVar>
   */
  Instruction arithmeticOperation = {0};
  arithmeticOperation.use_modrm = 1;
  arithmeticOperation.mod = MOD_REGISTER_DIRECT;

  if (varc2Prefix == 'v') {
    int srcRegCode = get_hardware_reg_index(varc2Prefix, idxVarc2);

    switch (op) {
      case '+':
        arithmeticOperation.opcode = OP_ADD_REG_TO_RM;
        break;
      case '-':
        arithmeticOperation.opcode = OP_SUB_REG_FROM_RM;
        break;
      case '*':
        arithmeticOperation.opcode = OP_IMUL_REG_BY_RM_STORE_IN_REG;
        // Special case for reg to reg multiplication: reg and r/m are swapped in ModRM byte
        int temp = dstRegCode;
        dstRegCode = srcRegCode;
        srcRegCode = temp;
        break;
      default:
        fprintf(stderr, "emit_arithmetic_operation: invalid operation: %c\n", op);
        return;
    }

    arithmeticOperation.reg = srcRegCode;
    arithmeticOperation.rm = dstRegCode;
  } else if (varc2Prefix == '$') {
    arithmeticOperation.isArithmOp = 1;
    arithmeticOperation.rm = dstRegCode;
    arithmeticOperation.use_imm = 1;
    arithmeticOperation.immediate = idxVarc2;

    /**
     * A tiny optimization:
     * if the immediate fits in a byte, use the appropriate instruction
     * emitting only 3 bytes: opcode, ModRM, and imm.
     * otherwise use the int one, emitting 6:
     * opcode, ModRM, imm, imm, imm, imm
     */
    int fitsInByte = (idxVarc2 >= -128 && idxVarc2 <= 127);
    arithmeticOperation.imm_size = fitsInByte ? 1 : 4;

    switch (op) {
      case '+':
      case '-': {
        // ADD and SUB share the same opcodes (0x83 for byte, 0x81 for int)
        arithmeticOperation.opcode = fitsInByte ? OP_IMM8_ARITHM_OP : OP_IMM32_ARITHM_OP;

        // Only difference lies in the reg field:
        arithmeticOperation.reg = (op == '+') ? EXT_ADD : EXT_SUB;
        break;
      }
      case '*': {
        arithmeticOperation.opcode = fitsInByte ? OP_IMUL_RM_BY_BYTE_STORE_IN_REG : OP_IMUL_RM_BY_INT_STORE_IN_REG;

        // IMUL uses the reg field for the destination
        arithmeticOperation.reg = dstRegCode;
        break;
      }
      default: {
        fprintf(stderr, "emit_arithmetic_operation: invalid operation: %c\n", op);
        return;
      }
    }
  } else {
    fprintf(stderr, "emit_arithmetic_operation: invalid varc2Prefix: %c\n", varc2Prefix);
    return;
  }
  emit_instruction(code, pos, &arithmeticOperation);
}

/**
 * Writes first instruction of a SBas conditional jump (`iflez`):
 * cmpl $0, <variableRegister>
 */
static void emit_cmp(unsigned char code[], int* pos, int varIndex) {
  int regCode = get_hardware_reg_index('v', varIndex);
  if (regCode == -1) return;

  Instruction cmp = {0};
  cmp.opcode = OP_IMM8_ARITHM_OP;
  cmp.isCmp = 1;
  cmp.use_modrm = 1;
  cmp.mod = MOD_REGISTER_DIRECT;
  cmp.reg = EXT_CMP;
  cmp.rm = regCode;

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
        return REG_RBX;
      case 2:
        return REG_R12;
      case 3:
        return REG_R13;
      case 4:
        return REG_R14;
      case 5:
        return REG_R15;
      default:
        fprintf(stderr, "get_hardware_reg_index: invalid variable index %c\n", idx);
        return -1;
    }
  } else if (type == 'p') {
    switch (idx) {
      case 1:
        return REG_RDI;
      case 2:
        return REG_RSI;
      case 3:
        return REG_RDX;
      default:
        fprintf(stderr, "get_hardware_reg_index: invalid param index %c\n", idx);
        return -1;
    }
  }
  fprintf(stderr, "get_hardware_reg_index: invalid type %c\n", type);
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

  if (inst->is_imm_mov) {
    // Case: Opcode embedding (e.g. 0xB8 + reg)
    // We only need REX.B if the register index is 8-15 (r8-r15)
    if (inst->imm_mov_rd > 7) {
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

    if (inst->is_imm_mov) {
      combinedOpcode += (inst->imm_mov_rd & 7);
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