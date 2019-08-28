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

static void *__attribute__ ((noinline)) orig_malloc(size_t size) {
  return (void *) 0;
}

static void *__attribute__ ((noinline)) orig_calloc(size_t num, size_t size) {
  return (void *) 0;
}

static void *__attribute__ ((noinline)) orig_realloc(void *ptr, size_t size) {
  return (void *) 0;
}

static void __attribute__ ((noinline)) orig_free(void *ptr) {
  return;
}
