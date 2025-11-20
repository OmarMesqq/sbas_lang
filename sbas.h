#ifndef SBAS_H
#define SBAS_H
#include <stdio.h>

#include "types.h"

/**
 * Compiles a SBas function
 * @param f **open** file handle of the `.sbas` file
 */
funcp sbasCompile(FILE *f);


/**
 * Frees the executable buffer of a SBas function
 * @param sbasFunc the SBas function pointer to free
 */
void sbasCleanup(funcp sbasFunc);

#endif