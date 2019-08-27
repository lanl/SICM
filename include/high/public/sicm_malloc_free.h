#pragma once

/* So that we can allocate/deallocate things in this library without recursively calling
 * our own definitions */
void (*libc_free)(void*);
void *(*libc_malloc)(size_t);
void *(*libc_calloc)(size_t, size_t);
void *(*libc_realloc)(void *, size_t);
