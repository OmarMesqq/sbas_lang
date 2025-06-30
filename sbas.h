/**
 * An SBas function:
 * Pointer to a function that takes `n` parameters (up to 3)
 * and returns a signed 32 integer
 */
typedef int (*funcp) ();

/**
 * Compiles a SBas function described in a .sbas file at
 * the open `FILE*` handle `f`, writing the SBas function
 * in the `codigo` buffer
 */
funcp sbasCompile(FILE *f, unsigned char codigo[]);