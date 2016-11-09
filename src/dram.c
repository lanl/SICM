#include "dram.h"

void* local_alloc(struct sicm_device* device, size_t size) {
  return numa_alloc_onnode(size, ((int*)device->payload)[0]);
}

void local_free(struct sicm_device* device, void* start, size_t size) {
  numa_free(start, size);
}

int local_add_to_bitmask(struct sicm_device* device, struct bitmask* mask) {
  numa_bitmask_setbit(mask, ((int*)device->payload)[0]);
  return 1;
}

int local_add(struct sicm_device* device_list, int idx, struct bitmask* numa_mask) {
  int i;
  for(i = 0; i <= numa_max_node(); i++) {
    if(!numa_bitmask_isbitset(numa_mask, i)) {
      ((int*)device_list[idx].payload)[0] = i;
      device_list[idx].alloc = local_alloc;
      device_list[idx].free = local_free;
      device_list[idx].add_to_bitmask = local_add_to_bitmask;
      idx++;
    }
  }
  return idx;
}

struct sicm_device_spec sicm_dram_spec() {
  struct sicm_device_spec spec;
  spec.non_numa_count = zero;
  spec.add_devices = local_add;
  return spec;
}
