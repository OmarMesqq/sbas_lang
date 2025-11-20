#include "linker.h"

#include "utils.h"

/**
 * Receives a buffer written with machine code and patches jump offsets to
 * correct locations
 *
 * @param code writable buffer
 * @param lt pointer to a line table struct
 * @param rt pointer to a relocation table struct
 * @param relocCount pointer to a counter for tracking lines with jumps
 *
 * @returns 0 on success, -1 on failure
 */
char sbasLink(unsigned char* code, LineTable* lt, RelocationTable* rt,
              int* relocCount) {
  for (int i = 0; i < *relocCount; i++) {
    if (lt[rt[i].lineTarget].line == 0) {
      compilationError("sbasCompile: jump target is not an executable line",
                       rt[i].lineTarget);
      return -1;
    }

    // get start of the line to jump to
    int targetOffset = lt[rt[i].lineTarget].offset;
    // get the location in buffer to jump from (the 4-byte placeholder to fill)
    int jumpFrom = rt[i].offset;

    /**
     * The CPU processes jumps as:
     * target_address = next_instruction_address + offset
     *
     * The `jle` instruction is 6 bytes wide: (2 for opcode, 4 for relative
     * offset)
     *
     * Currently, we are at the offset field (`jumpFrom`) since the 2 bytes
     * corresponding to the opcode were processed. Now, we add 4 to advance
     * through the 4 bytes of relative offset (`jumpFrom`) to get to the next
     * instruction.
     */
    int rel32 = targetOffset - (jumpFrom + 4);
    emitIntegerInHex(code, &jumpFrom, rel32);
  }
  return 0;
}