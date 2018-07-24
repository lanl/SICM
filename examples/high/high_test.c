#include <stdio.h>
#include <stdlib.h>
#include <time.h>

int main() {
  int i;
  struct timespec start, end;
  for(i = 0; i < 200; i++) {
    char* ptr = malloc(4096);
    if(!ptr) printf("failed to alloc at iteration %d\n", i);
    ptr[0] = 0;
    free(ptr);
  }
}
