#include <stdio.h>
#include <stdlib.h>
#include <time.h>

#define SIZE 2000

int main() {
  int i;
  char* ptr;

  ptr = malloc(sizeof(char) * SIZE);
  for(i = 0; i < SIZE; i++) {
    ptr[i] = 42;
  }
  free(ptr);
}
