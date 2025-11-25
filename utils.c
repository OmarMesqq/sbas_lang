#include "utils.h"

#include <stdio.h>

#include "config.h"
#include "types.h"

/**
 * Trims leading spaces (' ')/ 32 (dec)/ 0x20 (hex),
 * modifying `lineBuffer` in-place.
 * Runs in O(n)
 */
void trimLeadingSpaces(char* lineBuffer) {
  char* p = lineBuffer;
  // early return if string doesn't have leading whitespace
  if (*p != ' ') {
    return;
  }

  // count spaces
  unsigned spaces = 0;
  while (*p == ' ') {
    spaces++;
    p++;
  }

  char* aux = p;  // helper pointer with whitespace already consumed
  unsigned i = 0;
  while (*aux != '\0') {
#ifdef DEBUG
    printf("trimLeadingSpaces: setting lineBuffer[i = %d] (%c) to lineBuffer[spaces + i = %d] (%c)\n", i, lineBuffer[i], spaces + i, lineBuffer[spaces + i]);
#endif
    // shifts the entire string to beginning, eliminating spaces
    lineBuffer[i] = lineBuffer[spaces + i];
    i++;
    aux++;
  }

  // "discard" remaining bytes at end of string
  lineBuffer[i] = '\0';
}

/**
 * Prints the entire string `s`, followed by a character-by-character
 * dump of its contents (as character, decimal, and hex).
 */
void dumpString(char* s) {
  printf("%s", s);
  printf("dumpString: dumping string above...\n");
  char* p = s;
  while (*p != '\0') {
    printf("char: %c, %d (dec), %02x (hex)\n", *p, *p, *p);
    p++;
  }
  printf("\n");
}

/**
 * Converts the null-terminated string `str` to an integer.
 * Runs in O(n) - single pass.
 * Doesn't correctly handle broken inputs such as those with space, letters
 * and overflowing integers
 */
int stringToInt(char* str) {
  int num = 0;
  int digit = 0;
  int isNegative = 0;
  // is first char a minus sign?
  if (*str == '-') {
    isNegative = 1;
    str++;
  }
  while (*str != '\0') {
    // subtract the value of the digit char from 0 in ASCII table, getting its true value
    digit = *str - '0';
    // each new digit "adds" another power of 10 to the overall number
    num *= 10;
    num += digit;
    str++;
  }
  if (isNegative) {
    num *= -1;
  }

  return num;
}

/**
 * Receives a signed integer (32 bits on x86-64) in base 10 and writes it
 * in Little Endian hexadecimal in the buffer. Used for immediate values and
 * jump offsets.
 */
void emitIntegerInHex(unsigned char code[], int* pos, int integer) {
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
 * Dumps the `LineTable` corresponding to the currently compiled SBas file
 * @param lt pointer to the `LineTable`
 * @param lines amount of lines in the SBas file
 */
void printLineTable(LineTable* lt, int lines) {
  printf("----- START LINE TABLE -----\n");
  printf("%-14s %s\n", "LINE", "START OFFSET (dec)");
  for (int i = 1; i < lines; i++) {
    if (lt[i].line == 0) continue;
    printf("%-14d %d\n", lt[i].line, lt[i].offset);
  }
  printf("----- END LINE TABLE -----\n");
}

/**
 * Dumps the `RelocationTable` corresponding to the currently compiled SBas file
 * @param rt pointer to the `RelocationTable`
 * @param lines amount of lines in the SBas file
 */
void printRelocationTable(RelocationTable* rt, int relocCount) {
  printf("----- START RELOCATION TABLE -----\n");
  printf("%-20s %s\n", "PATCH OFFSET (dec)", "TARGET LINE");
  for (int i = 0; i < relocCount; i++) {
    printf("%-20d %d\n", rt[i].offset, rt[i].lineTarget);
  }
  printf("----- END RELOCATION TABLE -----\n");
}

/**
 * Prints a SBas compilation error `msg`, found at a given `line`, to `stderr`
 */
void compilationError(const char* msg, int line) {
  fprintf(stderr, "%s[line %d in .sbas file]: %s%s\n", RED, line, msg, RESET_COLOR);
}

/**
 * Attempts to create a file of name `filename`.
 */
FILE* createFile(const char* filename) {
  if (!filename) return NULL;
  FILE* fp = fopen(filename, "w");
  if (!fp) return NULL;
  return fp;
}

/**
 * Attempts to write the contents of `buf` to a file `f`
 * Ignores errors
 */
void writeToFile(unsigned char* buf, size_t size, FILE* f) {
  if (!f) return;
  fwrite(buf, sizeof(unsigned char), size, f);
}