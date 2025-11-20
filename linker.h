#ifndef LINKER_H
#define LINKER_H

#include "types.h"

char sbasLink(unsigned char* code, LineTable* lt, RelocationTable* rt,
              int* relocCount);

#endif