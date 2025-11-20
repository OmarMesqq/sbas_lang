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

/**
 * Contains information about a x86-64 general purpose register.
 *
 * Fields:
 * - reg_code: numeric code for register (0–7).
 * - rex: boolean flag signaling the need for a REX prefix byte (used in
 * extended registers 8 through 15).
 */
typedef struct {
  int reg_code;
  char rex;
} RegInfo;

typedef struct {
  unsigned int opcode;

  unsigned char is_64bit;  // 1 or 0

  unsigned char use_modrm;  // 1 or 0
  unsigned char mod;        // 2 bits (0-3)
  unsigned char reg;        // 4 bits (0-15)
  unsigned char rm;         // 4 bits (0-15)

  unsigned char use_disp;
  int displacement;  // -8, -16, etc.

  unsigned char use_imm;
  int immediate;           // 10, 100, etc.
  unsigned char imm_size;  // 1 or 4 bytes
} Instruction;

#endif