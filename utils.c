#include "utils.h"
#include <stdio.h>

// #define DEBUG

/**
 * Trims leading spaces (' ')/ 32 (dec)/ 0x20 (hex),
 * modifying `lineBuffer` in-place.
 * Runs in O(n)
 */
void trim_leading_spaces(char* lineBuffer) {
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
    printf("trim_leading_spaces: setting lineBuffer[i = %d] (%c) to lineBuffer[spaces + i = %d] (%c)\n", i, lineBuffer[i], spaces + i, lineBuffer[spaces + i]);
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
void dump_str(char* s) {
  printf("%s", s);
  printf("dump_str: dumping string above...\n");
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
  while(*str != '\0') {
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