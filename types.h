#ifndef TYPES_H
#define TYPES_H

/**
 * The SBas function:
 * Pointer to a function that takes `n` parameters (up to 3)
 * and returns a signed integer (32 bits)
 */
typedef int (*funcp)();

/**
 * Maps each line in `.sbas` file to its offset in the machine code buffer
 *
 * Fields:
 * - line: line in `.sbas` file
 * - offset: start of the line's instructions in the buffer
 */
typedef struct {
  unsigned char line;
  int offset;
} LineTable;

/**
 * Maps placeholders for conditional jumps at a given `offset` in the buffer
 * to the desired `lineTarget` in the `.sbas` file to jump to
 *
 * Fields:
 * - offset: position in the buffer where the jump offset needs to be patched to
 * jump to `lineTarget`
 * - lineTarget: line to jump to
 */
typedef struct {
  int offset;
  unsigned char lineTarget;
} RelocationTable;

typedef struct {
  unsigned int opcode;

  unsigned char is_64bit;

  unsigned char use_modrm;
  unsigned char mod;  // specifies addressing mode
  unsigned char reg;  // source
  unsigned char rm;   // destination

  unsigned char use_disp;
  int displacement;  // -8, -16, etc.

  unsigned char use_imm;
  int immediate;           // 10, 100, etc.
  unsigned char imm_size;  // 1 or 4 bytes

  // mov imm32/64, r32/64 condenses the register id (rd) inside the opcode
  unsigned char is_imm_mov;
  int imm_mov_rd;

  unsigned char isArithmOp;
  unsigned char isCmp;
} Instruction;

#endif