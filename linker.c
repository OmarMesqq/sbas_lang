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
char sbasLink(unsigned char* code, LineTable* lt, RelocationTable* rt, int* relocCount) {
  for (int i = 0; i < *relocCount; i++) {
    /**
     * The current plead for writing a jump at `offsetToPatch` to `targetLine`
     */
    RelocationTable relocationRequest = rt[i];
    int offsetToPatch = relocationRequest.offset;
    const unsigned targetLine = relocationRequest.targetLine;

    // Look up the target in the LineTable
    LineTable relocationTarget = lt[targetLine];
    const char lineExists = relocationTarget.line == 0 ? 0 : 1;
    if (!lineExists) {
      compilationError("sbasLink: jump target is not an executable line", targetLine);
      return -1;
    }

    // the target line's offset in the buffer is the address we want to jump to
    const int targetAddress = relocationTarget.offset;

    /**
     * The instruction right after the current one (jump) is obtained simply
     * by adding enough bytes to "read past" the position we are currently at in the buffer.
     *
     * The current position is exactly the desired `offsetToPatch` the linker
     * is fixing up, whilst the amount of bytes to "read past" it is dictated
     * by the opcode/ISA.
     *
     * As we are currently doing a 32 bit jump, to get the next instruction's
     * address we add 4 bytes after it:
     * nextInstructionAddress = offset field + offset's size (4 bytes)
     *
     * In particular, we reach at the "formula":
     * `nextInstructionAddress = offsetToPatch + 4`
     */
    const int nextInstructionAddress = offsetToPatch + 4;

    /**
     * Suppose your code "wants to" jump to a `targetAddress` - here, the
     * target line's offset in the buffer.
     *
     * However, the CPU cannot just "go to" `targetAddress`.
     *
     * Why? Once it "understands our intent" to deviate control flow
     * somewhere else, i.e. once it fully decodes the current (jump) instruction,
     * the instruction pointer (IP) is already consumed past it,
     * landing on the instruction immediately after.
     *
     * Now the IP is at `nextInstructionAddress`, and since a jump is insomuch
     * a matter of incrementing or decrementing it, the CPU doesn't "talk" in
     * absolute regarding `targetAddress`. Rather it "asks" how can it get
     * to the user's desired `targetAddress`
     * given that it's at `nextInstructionAddress`?
     *
     * So what we are really dealing with here is:
     * `targetAddress = nextInstructionAddress + someValue`
     *
     * You just have to supply `someValue` to the CPU
     * so it goes where you want to :)
     *
     * Rearranging the formula above:
     * `someValue = targetAddress - nextInstructionAddress`
     *
     * Obviously, this value can be negative, allowing the instruction pointer to
     * go "back and forth" in the program's text section. For 32-bit integers,
     * the span of possible values is divided in two ranges:
     * - `0 to +2,147,483,647` (00000000 to 7FFFFFFF in hex)
     * - `-2,147,483,648 to -1` (80000000 to FFFFFFFF in hex)
     *
     * In the first case, we have a forward jump where the CPU adds the offset
     * to the IP, incrementing it and landing "ahead" of the current instruction.
     *
     * Due to two's complement, the logic is the same for the latter case.
     * If the most significant bit is 1, meaning the most significant byte
     * is in the range 8 to F, we're dealing with a negative number.
     *
     * If we wanted to find its absolute value:
     * 1. flip its bits: `~offset`
     * 2. add one: `offset + 1`
     *
     * In reality, the CPU doesn't do this and then subtract the number's magnitude.
     * Rather, it simply adds the integer to the next instruction address and any overflowing
     * `1`'s are discarded. The final value "rolls over" so that the offset to be added
     * to IP is actually less than the value it is at right now. Therefore, the IP
     * is decremented, landing at an instruction "behind" the current one.
     *
     * Finally, since `someValue` is relative (not absolute!) and is a difference
     * between two signed integers (32 bit wide), we call it `rel32`, thus
     * arriving at:
     *
     * `rel32 = targetAddress - nextInstructionAddress`
     */
    const int rel32 = targetAddress - nextInstructionAddress;

    emitIntegerInHex(code, &offsetToPatch, rel32);
  }
  return 0;
}
