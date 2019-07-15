#ifndef OP_NEW_DEL
#define OP_NEW_DEL

#include <string.h>
#include "sicm_high.h"

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
