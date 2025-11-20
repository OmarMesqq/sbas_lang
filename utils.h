#ifndef UTILS_H
#define UTILS_H
#include "types.h"

void trim_leading_spaces(char* lineBuffer);
void dump_str(char* s);
int stringToInt(char* str);
void compilation_error(const char* msg, int line);
void emit_integer_in_hex(unsigned char code[], int* pos, int integer);
void print_line_table(LineTable* lt, int lines);
void print_relocation_table(RelocationTable* rt, int relocCount);

#endif