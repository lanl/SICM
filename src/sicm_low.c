#include "sicm_low.h"
#include "dram.h"
#include "knl_hbm.h"

#include <numaif.h>

void* sicm_alloc(struct sicm_device* device, size_t size) {
  return device->alloc(device, size);
}

void sicm_free(struct sicm_device* device, void* ptr, size_t size) {
  device->free(device, ptr, size);
}

size_t sicm_used(struct sicm_device* device) {
  return device->used(device);
}

size_t sicm_capacity(struct sicm_device* device) {
  return device->capacity(device);
}

int sicm_model_distance(struct sicm_device* device) {
  return device->model_distance(device);
}

void sicm_latency(struct sicm_device* device, struct sicm_timing* res) {
  return device->latency(device, res);
}

int sicm_add_to_bitmask(struct sicm_device* device, struct bitmask* mask) {
  return device->add_to_bitmask(device, mask);
}

int sicm_move(struct sicm_device* src, struct sicm_device* dest, void* ptr, size_t len) {
  if(src->move_ty == SICM_MOVER_NUMA && dest->move_ty == SICM_MOVER_NUMA) {
    int dest_node = dest->move_payload.numa;
    nodemask_t nodemask;
    nodemask_zero(&nodemask);
    nodemask_set_compat(&nodemask, dest_node);
    return mbind(ptr, len, MPOL_BIND, nodemask.n, numa_max_node() + 2, MPOL_MF_MOVE);
  }
  return -1;
}

int zero() {
  return 0;
}

struct bitmask* sicm_cpu_mask_memo = NULL;

struct bitmask* sicm_cpu_mask() {
  if(sicm_cpu_mask_memo) return sicm_cpu_mask_memo;
  
  struct bitmask* cpumask = numa_allocate_cpumask();
  int cpu_count = numa_num_possible_cpus();
  int node_count = numa_max_node() + 1;
  sicm_cpu_mask_memo = numa_bitmask_alloc(node_count);
  int i, j;
  for(i = 0; i < node_count; i++) {
    numa_node_to_cpus(i, cpumask);
    for(j = 0; j < cpu_count; j++) {
      if(numa_bitmask_isbitset(cpumask, j)) {
        numa_bitmask_setbit(sicm_cpu_mask_memo, i);
        break;
      }
    }
  }
  numa_free_cpumask(cpumask);
  return sicm_cpu_mask_memo;
}

int main() {
  int spec_count = 2;
  struct sicm_device_spec specs[] = {sicm_knl_hbm_spec(), sicm_dram_spec()};
  
  int i;
  int non_numa = 0;
  for(i = 0; i < spec_count; i++)
    non_numa += specs[i].non_numa_count();
  
  int device_count = non_numa + numa_max_node() + 1;
  struct sicm_device* devices = malloc(device_count * sizeof(struct sicm_device));
  struct bitmask* numa_mask = numa_bitmask_alloc(numa_max_node() + 1);
  int idx = 0;
  for(i = 0; i < spec_count; i++)
    idx = specs[i].add_devices(devices, idx, numa_mask);
  numa_bitmask_free(numa_mask);
  
  /*
   * test code starts here
   * everything above this comment is required spinup
   */
  printf("used: %lu\n", sicm_used(&devices[0]));
  printf("capacity: %lu\n", sicm_capacity(&devices[0]));
  int count = 1000000;
  int* test = sicm_alloc(&devices[0], count * sizeof(int));
  for(i = 0; i < count; i++)
    test[i] = i;
  printf("%p\n", sicm_cpu_mask());
  printf("%p\n", sicm_cpu_mask());
  struct sicm_timing timing;
  sicm_latency(&devices[0], &timing);
  printf("%u %u %u %u\n", timing.alloc, timing.write, timing.read, timing.free);
  /*char path[100];
  sprintf(path, "cat /proc/%d/numa_maps", (int)getpid());
  system(path);
  sicm_move(&devices[0], &devices[1], test, count * sizeof(int));
  system(path);
  sicm_free(&devices[1], test, count * sizeof(int));*/
  return 1;
}
