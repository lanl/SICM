#include "sg.h"
#include <stdio.h>
#include <time.h>

int main() {
  sg_init(0);
  int i;
  struct timespec start, end;
  clock_gettime(CLOCK_MONOTONIC_RAW, &start);
  for(i = 0; i < 200; i++) {
    char* ptr = sg_alloc_exact(4096);
    if(!ptr) printf("failed to alloc at iteration %d\n", i);
    ptr[0] = 0;
  }
  clock_gettime(CLOCK_MONOTONIC_RAW, &end);
  size_t delta = (end.tv_sec - start.tv_sec) * 1000000 + (end.tv_nsec - start.tv_nsec) / 1000;
  printf("sg time: %lu\n", delta);

  clock_gettime(CLOCK_MONOTONIC_RAW, &start);
  for(i = 0; i < 200; i++) {
    char* ptr = sg_alloc_exact(4096);
    if(!ptr) printf("failed to alloc at iteration %d\n", i);
    ptr[0] = 0;
  }
  clock_gettime(CLOCK_MONOTONIC_RAW, &end);
  delta = (end.tv_sec - start.tv_sec) * 1000000 + (end.tv_nsec - start.tv_nsec) / 1000;
  printf("sg time: %lu\n", delta);

  clock_gettime(CLOCK_MONOTONIC_RAW, &start);
  for(i = 0; i < 200; i++) {
    char* ptr = sg_alloc_exact(4096);
    if(!ptr) printf("failed to alloc at iteration %d\n", i);
    ptr[0] = 0;
  }
  clock_gettime(CLOCK_MONOTONIC_RAW, &end);
  delta = (end.tv_sec - start.tv_sec) * 1000000 + (end.tv_nsec - start.tv_nsec) / 1000;
  printf("sg time: %lu\n", delta);

  clock_gettime(CLOCK_MONOTONIC_RAW, &start);
  for(i = 0; i < 200; i++) {
    char* ptr = malloc(4096);
    if(!ptr) printf("failed to alloc at iteration %d\n", i);
    ptr[0] = 0;
  }
  clock_gettime(CLOCK_MONOTONIC_RAW, &end);
  delta = (end.tv_sec - start.tv_sec) * 1000000 + (end.tv_nsec - start.tv_nsec) / 1000;
  printf("malloc time: %lu\n", delta);
  /*printf("%d\n", sg_performance_list.count);
  int i;
  for(i = 0; i < sg_performance_list.count; i++) {
    printf("page size: %d\n", sicm_device_page_size(&sg_performance_list.devices[i]));
  }
  for(i = 0; i < 5; i++) {
    printf("%lu, %lu, %lu\n", sicm_avail(&sg_capacity_list.devices[0]), sicm_avail(&sg_capacity_list.devices[1]), sicm_avail(&sg_capacity_list.devices[2]));
    char* blob = sg_alloc_exact(10000000);
    printf("%p\n", blob);
    size_t i;
    for(i = 0; i < 10000000; i++) blob[i] = (i % ('z' - 'A')) + 'A';
    printf("%lu, %lu, %lu\n", sicm_avail(&sg_capacity_list.devices[0]), sicm_avail(&sg_capacity_list.devices[1]), sicm_avail(&sg_capacity_list.devices[2]));
    sg_free(blob);
    printf("%lu, %lu, %lu\n", sicm_avail(&sg_capacity_list.devices[0]), sicm_avail(&sg_capacity_list.devices[1]), sicm_avail(&sg_capacity_list.devices[2]));
  }*/
}
