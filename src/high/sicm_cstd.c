#ifndef OP_NEW_DEL
#define OP_NEW_DEL

#include <string.h>
#include "sicm_high.h"

/* Just define `malloc`, `calloc`, `realloc`, and `free`. We want
 * all allocations to come through us no matter what, else we'll have edge cases
 * where a stdlib call allocates memory without having been transformed by our compiler
 * pass. In some cases this results in a `malloc` call in a shared library (which uses `libc`'s `malloc`),
 * and an inlined `free` call which gets transformed by our compiler wrappers.
 */
void *malloc(size_t size) {
  return sh_alloc(0, size);
}

void *calloc(size_t num, size_t size) {
  return sh_calloc(0, num, size);
}

void *realloc(void *ptr, size_t new_size) {
  return sh_realloc(0, ptr, new_size);
}

void free(void *ptr) {
  sh_free(ptr);
}

/* I'm commenting these out because external libraries (such as libpfm4) will
 * use these function to allocate memory, then use `libc`'s `free` to free up the
 * memory allocated with them, resulting in an invalid pointer (since the pointer
 * was allocated with jemalloc and freed with `libc`).
 */
#if 0
/* Never inline these */
#undef strdup
char *strdup(const char *str1) __attribute__((used)) __attribute__((noinline));

/* Call sh_alloc from all of these */
char *strdup(const char *str1) {
  char *buf = sh_alloc(0, strlen(str1) + 1);
  strcpy(buf, str1);
  return buf;
}
#endif

#endif
