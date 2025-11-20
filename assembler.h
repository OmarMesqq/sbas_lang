#ifndef ASSEMBLER_H
#define ASSEMBLER_H

#include <stdio.h>
#include "types.h"

char sbasAssemble(unsigned char* code, FILE* f, LineTable* lt,
                 RelocationTable* rt, int* relocCount);

#endif