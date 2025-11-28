#include "sbas.h"

#include <stdlib.h>
#include <sys/mman.h>
#include <unistd.h>

#include "assembler.h"
#include "config.h"
#include "linker.h"
#include "types.h"
#include "utils.h"

#define MAX_CODE_SIZE 1024  // maximum bytes the buffer holds

static void* alloc_writable_buffer(size_t size);
static int make_buffer_executable(void* ptr, size_t size);

/**
 * Compiles a SBas function described in a .sbas file at
 * the open `FILE*` handle `f`
 */
funcp sbasCompile(FILE* f) {
  char assembleRet = 0;        // result of SBas assembling to machine code
  char linkRet = 0;            // result of machine code fixup patching
  int relocCount = 0;          // lines with jump offsets
  unsigned char* code = NULL;  // buffer to write SBas logic
  funcp result_func = NULL;    // return result: `code` buffer casted to SBas function
  int mprotectRes = 0;         // holds status of syscall to make buffer executable
  LineTable* lt = NULL;
  RelocationTable* rt = NULL;

  // Edge case handling: empty file
  fseek(f, 0, SEEK_END);  // Seek to the end of the file
  long size = ftell(f);   // Get the current file position: its size
  if (size == 0) {
    fprintf(stderr, "sbasCompile: the provided SBas file is empty. Aborting!\n");
    goto on_error;
  } else {
    rewind(f);  // go back to file's start
  }

  lt = calloc((MAX_LINES + 1), sizeof(LineTable));
  rt = calloc((MAX_LINES + 1), sizeof(RelocationTable));
  if (!lt || !rt) {
    fprintf(stderr, "sbasCompile: failed to alloc line and/or relocation table!\n");
    goto on_error;
  }

  code = alloc_writable_buffer(MAX_CODE_SIZE);
  if (!code) {
    fprintf(stderr, "sbasCompile: failed to alloc writable memory.\n");
    goto on_error;
  }

  /**
   * First pass: emit most instructions and leave 4-byte placeholders for jumps
   */
  assembleRet = sbasAssemble(code, f, lt, rt, &relocCount);
  if (assembleRet == -1) {
    goto on_error;
  }

  /**
   * Second pass: fills 4-byte placeholder with offsets
   */
  linkRet = sbasLink(code, lt, rt, &relocCount);
  if (linkRet == -1) {
    goto on_error;
  }

  mprotectRes = make_buffer_executable(code, MAX_CODE_SIZE);
  if (mprotectRes == -1) {
    fprintf(stderr, "sbasCompile: failed to make_buffer_executable\n");
    goto on_error;
  }

  result_func = (funcp)code;
  goto on_cleanup;

/**
 * This label is reached only on errors:
 * Frees SBas buffer if defined, falls through auxiliary data structures `free`s
 * and return result
 */
on_error:
  if (code) {
    sbasCleanup((funcp)code);
  }
/**
 * This label is hit by both success and error paths:
 * `free`s auxiliary data structures and falls through
 * return result
 */
on_cleanup:
  // It's safe to call free on NULL
  free(lt);
  free(rt);

  return result_func;  // Returns the buffer with SBas code, `NULL` otherwise
}

/**
 * Frees the executable buffer of a SBas function `sbasFunc`
 */
void sbasCleanup(funcp sbasFunc) { munmap((void*)sbasFunc, MAX_CODE_SIZE); }

/**
 * Allocates a RW buffer for emitting machine code
 * corresponding to SBas code semantics
 */
static void* alloc_writable_buffer(size_t size) {
  // Round to page size
  size_t pagesize = sysconf(_SC_PAGESIZE);

  size_t alloc_size = ((size + pagesize - 1) / pagesize) * pagesize;
  void* ptr = mmap(NULL, alloc_size, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
  if (ptr == MAP_FAILED) {
    fprintf(stderr, "alloc_writable_buffer: failed to mmap writable buffer.\n");
    return NULL;
  }

  return ptr;
}

/**
 * Drops write flag of SBas code buffer after done emitting, enforcing W^X
 */
static int make_buffer_executable(void* ptr, size_t size) {
  size_t pagesize = sysconf(_SC_PAGESIZE);
  size_t alloc_size = ((size + pagesize - 1) / pagesize) * pagesize;

  // change protection to R+X (drop Write)
  if (mprotect(ptr, alloc_size, PROT_READ | PROT_EXEC) != 0) {
    fprintf(stderr, "make_buffer_executable: failed to set buffer to R+X through mprotect.\n");
    return -1;
  }

  return 0;
}
