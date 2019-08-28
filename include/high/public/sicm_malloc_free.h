#pragma once
#include <stdlib.h> /* For size_t */

#if 0
#ifdef SICM_RUNTIME
  /* So that we can allocate/deallocate things in this library without recursively calling
   * our own definitions. If this is included from SICM's runtime library, these will be defined
   * in sh_init() with dlsym.
   */
  void (*libc_free)(void*);
  void *(*libc_malloc)(size_t);
  void *(*libc_calloc)(size_t, size_t);
  void *(*libc_realloc)(void *, size_t);
#else
  /* Otherwise, just use whatever malloc or free */
  #define libc_free free
  #define libc_malloc malloc
  #define libc_calloc calloc
  #define libc_realloc realloc
#endif
#endif

#ifdef SICM_RUNTIME
  /* For the runtime library, we're overriding 'malloc' and 'free', so we need to grab libc's
   * 'malloc' and 'free'. Since this header is included multiple times in the runtime library,
   * define these in sicm_runtime.c.
   */
void *__attribute__ ((noinline)) orig_malloc(size_t size);
void *__attribute__ ((noinline)) orig_calloc(size_t num, size_t size);
void *__attribute__ ((noinline)) orig_realloc(void *ptr, size_t size);
void __attribute__ ((noinline)) orig_free(void *ptr);

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
