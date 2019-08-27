#pragma once

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
