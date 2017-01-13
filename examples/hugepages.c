#include "sicm_low.h"
#include <stdio.h>
#include <time.h>

// 20 MiB
#define SZ 20971520

int main() {
  struct sicm_device_list devices = sicm_init();
  unsigned int start, end;
  unsigned int i;
  
  char* data = sicm_alloc(&devices.devices[0], SZ);
  start = time(NULL);
  //#pragma omp parallel for
  for(i = 0; i < 1000000000; i++) {
    data[sicm_hash(i) % SZ] = '0';
  }
  end = time(NULL);
  sicm_free(&devices.devices[0], data, SZ);
  printf("time for normal pages: %d s\n", end - start);
  
  data = sicm_alloc(&devices.devices[1], SZ);
  start = time(NULL);
  //#pragma omp parallel for
  for(i = 0; i < 1000000000; i++) {
    data[sicm_hash(i) % SZ] = '0';
  }
  end = time(NULL);
  sicm_free(&devices.devices[1], data, SZ);
  printf("time for huge pages: %d s\n", end - start);
}
