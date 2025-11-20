#ifndef UTILS_H
#define UTILS_H
#include <stdio.h>

#include "types.h"

void trimLeadingSpaces(char* lineBuffer);
void dumpString(char* s);
int stringToInt(char* str);
void compilationError(const char* msg, int line);
void emitIntegerInHex(unsigned char code[], int* pos, int integer);
void printLineTable(LineTable* lt, int lines);
void printRelocationTable(RelocationTable* rt, int relocCount);
FILE* createFile(const char* filename);
void writeToFile(unsigned char* buf, size_t size, FILE* f);
#endif