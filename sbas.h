#ifndef SBAS_H
#define SBAS_H
#include <stdio.h>

/**
 * The SBas function:
 * Pointer to a function that takes `n` parameters (up to 3)
 * and returns a signed integer (32 bits)
 */
typedef int (*funcp) ();

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