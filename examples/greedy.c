#include "sg.h"
#include <stdio.h>

int main() {
  sg_init(0);
  char* blob = sg_alloc_cap(10000);
  int i;
  for(i = 0; i < 10000; i++) blob[i] = (i % ('z' - 'A')) + 'A';
  for(i = 0; i < 10000; i++) printf("%c", blob[i]);
  printf("\n");
}
