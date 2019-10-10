#ifndef OP_NEW_DEL
#define OP_NEW_DEL

#include <stdlib.h> /* For exit() */
#include <string.h>
#include "sicm_runtime.h" /* For sh_initialized */

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

external int posix_memalign(void **ptr, size_t alignment, size_t size) {
  return sh_posix_memalign(0, ptr, alignment, sz);
}

external void * aligned_alloc(size_t alignment, size_t size) {
  return sh_aligned_alloc(0, alignment, size);
}

external void * memalign(size_t alignment, size_t size) {
  return sh_memalign(0, alignment, size);
}

#endif
