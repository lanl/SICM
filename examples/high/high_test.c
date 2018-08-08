#include <stdio.h>
#include <stdlib.h>
#include <time.h>

#define SIZE 1000000000

int main() {
  int i;
  char* data, c;


  /* Do some work */
  data = malloc(sizeof(char) * SIZE);
  for(i = 0; i < SIZE; i++) {
    data[i] = i;
  }
  for(i = 1; i < SIZE; i++) {
    data[i] = data[i - 1];
  }
  for(i = 0; i < SIZE; i++) {
    c = data[i];
    c = c + 1;
  }

  printf("DATA: %p -> %p\n", data, ((char *)data) + SIZE);
  printf("I: %p\n", &i);
  printf("C: %p\n", &c);

  free(data);
}
