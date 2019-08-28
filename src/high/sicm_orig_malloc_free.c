#include "sicm_malloc_free.h"

/* Only for sicm_hotset and sicm_memreserve. This defines orig_malloc to simply call
 * malloc, which will be overriden by any object files in the runtime library. This should
 * not happen, so only link this file against files that don't override malloc and free.
 */

void *__attribute__ ((noinline)) orig_malloc(size_t size) {
  return malloc(size);
}

void *__attribute__ ((noinline)) orig_calloc(size_t num, size_t size) {
  return calloc(num, size);
}

void *__attribute__ ((noinline)) orig_realloc(void *ptr, size_t size) {
  return realloc(ptr, size);
}

void __attribute__ ((noinline)) orig_free(void *ptr) {
  free(ptr);
  return;
}
