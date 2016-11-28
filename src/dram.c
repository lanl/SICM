#include "dram.h"
#include "numa_common.h"

int sicm_dram_add(struct sicm_device* device_list, int idx, struct bitmask* numa_mask) {
  int i;
  for(i = 0; i <= numa_max_node(); i++) {
    if(!numa_bitmask_isbitset(numa_mask, i)) {
      device_list[idx].ty = SICM_DRAM;
      device_list[idx].move_ty = SICM_MOVER_NUMA;
      device_list[idx].move_payload.numa = i;
      device_list[idx].alloc = sicm_numa_common_alloc;
      device_list[idx].free = sicm_numa_common_free;
      device_list[idx].used = sicm_numa_common_used;
      device_list[idx].capacity = sicm_numa_common_capacity;
      device_list[idx].model_distance = sicm_numa_common_model_distance;
      device_list[idx].latency = sicm_numa_common_latency;
      device_list[idx].add_to_bitmask = sicm_numa_common_add_to_bitmask;
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
