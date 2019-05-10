#ifndef OP_NEW_DEL
#define OP_NEW_DEL

#include <string.h>
#include "sicm_high.h"

/* Never inline these */
#undef strdup
char *strdup(const char *str1) __attribute__((used)) __attribute__((noinline));

/* Call sh_alloc from all of these */
char *strdup(const char *str1) {
  printf("Allocating from strdup: %s\n", str1);
  char *buf = sh_alloc(0, strlen(str1) + 1);
  strcpy(buf, str1);
  return buf;
}

#endif
