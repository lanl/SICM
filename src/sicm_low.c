#include "sicm_low.h"
#include "dram.h"
#include "knl_hbm.h"

#include <stdio.h>

void* sicm_alloc(struct sicm_device* device, size_t size) {
  return device->alloc(device, size);
}

void sicm_free(struct sicm_device* device, void* ptr, size_t size) {
  device->free(device, ptr, size);
}

int sicm_add_to_bitmask(struct sicm_device* device, struct bitmask* mask) {
  return device->add_to_bitmask(device, mask);
}

int zero() {
  return 0;
}

int main() {
  int spec_count = 2;
  struct sicm_device_spec specs[] = {sicm_knl_hbm_spec(), sicm_dram_spec()};
  
  int i;
  int non_numa = 0;
  for(i = 0; i < spec_count; i++)
    non_numa += specs[i].non_numa_count();
  
  printf("%d\n", numa_max_node());
  int device_count = non_numa + numa_max_node() + 1;
  struct sicm_device* devices = malloc(device_count * sizeof(struct sicm_device));
  struct bitmask* numa_mask = numa_bitmask_alloc(numa_max_node() + 1);
  int idx = 0;
  for(i = 0; i < spec_count; i++)
    idx = specs[i].add_devices(devices, idx, numa_mask);
  
  int* test = sicm_alloc(&devices[0], 100 * sizeof(int));
  for(i = 0; i < 100; i++)
    test[i] = i;
  for(i = 0; i < 100; i++)
    printf("%d ", test[i]);
  printf("\n");
  sicm_free(&devices[0], test, 100 * sizeof(int));
  return 1;
}
