#include "sg.h"
#include <stdio.h>

int main() {
  sg_init(0);
  printf("%d\n", sg_performance_list.count);
  int i;
  for(i = 0; i < sg_performance_list.count; i++) {
    printf("page size: %d\n", sicm_device_page_size(&sg_performance_list.devices[i]));
  }
  for(i = 0; i < 5; i++) {
    printf("%lu, %lu, %lu\n", sicm_avail(&sg_capacity_list.devices[0]), sicm_avail(&sg_capacity_list.devices[1]), sicm_avail(&sg_capacity_list.devices[2]));
    char* blob = sg_alloc_perf(10000000);
    size_t i;
    for(i = 0; i < 10000000; i++) blob[i] = (i % ('z' - 'A')) + 'A';
    printf("%lu, %lu, %lu\n", sicm_avail(&sg_capacity_list.devices[0]), sicm_avail(&sg_capacity_list.devices[1]), sicm_avail(&sg_capacity_list.devices[2]));
    sg_free(blob);
    printf("%lu, %lu, %lu\n", sicm_avail(&sg_capacity_list.devices[0]), sicm_avail(&sg_capacity_list.devices[1]), sicm_avail(&sg_capacity_list.devices[2]));
  }
}
