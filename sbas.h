#include <stdio.h>

/**
 * A SBas function:
 * Pointer to a function that takes `n` parameters (up to 3)
 * and returns a signed 32 integer
 */
typedef int (*funcp) ();

/**
 * Compiles a SBas function described in a .sbas file at
 * the open `FILE*` handle `f`
 */
funcp sbasCompile(FILE *f);


/**
 * Frees the executable buffer of a SBas function `sbasFunc`
 */
void sbasCleanup(funcp sbasFunc);