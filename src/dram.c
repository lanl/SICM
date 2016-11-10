#include "dram.h"

void* sicm_dram_alloc(struct sicm_device* device, size_t size) {
  return numa_alloc_onnode(size, ((int*)device->payload)[0]);
}

void sicm_dram_free(struct sicm_device* device, void* start, size_t size) {
  numa_free(start, size);
}

int sicm_dram_add_to_bitmask(struct sicm_device* device, struct bitmask* mask) {
  numa_bitmask_setbit(mask, ((int*)device->payload)[0]);
  return 1;
}

int sicm_dram_add(struct sicm_device* device_list, int idx, struct bitmask* numa_mask) {
  int i;
  for(i = 0; i <= numa_max_node(); i++) {
    if(!numa_bitmask_isbitset(numa_mask, i)) {
      ((int*)device_list[idx].payload)[0] = i;
      device_list[idx].ty = SICM_DRAM;
      device_list[idx].alloc = sicm_dram_alloc;
      device_list[idx].free = sicm_dram_free;
      device_list[idx].add_to_bitmask = sicm_dram_add_to_bitmask;
      idx++;
    }
  }
  return idx;
}

struct sicm_device_spec sicm_dram_spec() {
  struct sicm_device_spec spec;
  spec.non_numa_count = zero;
  spec.add_devices = sicm_dram_add;
  return spec;
}
