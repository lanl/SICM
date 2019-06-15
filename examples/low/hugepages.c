#include "sicm_low.h"
#include "sicm_impl.h"
#include <stdio.h>
#include <time.h>
#include <sys/mman.h>
#ifndef MAP_HUGE_SHIFT
#include <linux/mman.h>
#endif

// 20 MiB
#define SZ 20971520

int main() {
  struct sicm_device_list devices = sicm_init();
  unsigned int start, end;
  unsigned int i;

  printf("%d\n", MAP_HUGE_SHIFT);

  char* data = sicm_device_alloc(&devices.devices[0], SZ);
  start = time(NULL);
  //#pragma omp parallel for
  for(i = 0; i < 1000000000; i++) {
    data[sicm_hash(i) % SZ] = '0';
  }
  end = time(NULL);
  sicm_device_free(&devices.devices[0], data, SZ);
  printf("time for normal pages: %d s\n", end - start);

  data = sicm_device_alloc(&devices.devices[0], SZ);
  start = time(NULL);
  //#pragma omp parallel for
  for(i = 0; i < 1000000000; i++) {
    data[sicm_hash(i) % SZ] = '0';
  }
  end = time(NULL);
  sicm_device_free(&devices.devices[1], data, SZ);
  printf("time for huge pages: %d s\n", end - start);
}
