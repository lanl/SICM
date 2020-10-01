#pragma once
#include <stdlib.h> /* For size_t */

#ifdef SICM_RUNTIME
  /* For the runtime library, we're overriding 'malloc' and 'free', so we need to grab libc's
   * 'malloc' and 'free'. Since this header is included multiple times in the runtime library,
   * define these in sicm_runtime.c.
   */
void *__attribute__ ((noinline)) orig_malloc(size_t size);
void *__attribute__ ((noinline)) orig_calloc(size_t num, size_t size);
void *__attribute__ ((noinline)) orig_realloc(void *ptr, size_t size);
void __attribute__ ((noinline)) orig_free(void *ptr);
void *__attribute__ ((noinline)) orig_valloc(size_t size);

#else
  /* For other functions that use sicm_parsing.h or sicm_tree.h, just make these call normal
   * 'malloc' and 'free'.
   */
static void *__attribute__ ((noinline)) orig_malloc(size_t size) {
  return malloc(size);
}

static void *__attribute__ ((noinline)) orig_calloc(size_t num, size_t size) {
  return calloc(num, size);
}

static void *__attribute__ ((noinline)) orig_realloc(void *ptr, size_t size) {
  return realloc(ptr, size);
}

static void __attribute__ ((noinline)) orig_free(void *ptr) {
  free(ptr);
  return;
}
#endif
