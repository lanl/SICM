#include "sicm_low.h"
#include "dram.h"
#include "knl_hbm.h"

#include <numaif.h>

/// Allocates memory on the given device.
/**
 * This is just a wrapper around device->alloc, to deal with the problem
 * that C can't do the implicit object closure.
 */
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

size_t sicm_triad_kernel_linear(double* a, double* b, double* c, size_t size) {
  int i;
  double scalar = 3.0;
  #pragma omp parallel for
  for(i = 0; i < size; i++) {
    a[i] = b[i] + scalar * c[i];
  }
  return size * 3 * sizeof(double);
}

size_t sicm_triad_kernel_random(double* a, double* b, double* c, size_t* indexes, size_t size) {
  int i, idx;
  double scalar = 3.0;
  #pragma omp parallel for
  for(i = 0; i < size; i++) {
    idx = indexes[i];
    a[idx] = b[idx] + scalar * c[idx];
  }
  return size * (sizeof(size_t) + 3 * sizeof(double));
}

void sicm_latency(struct sicm_device* device, struct sicm_timing* res) {
  struct timespec start, end;
  int i;
  char b;
  unsigned int n = time(NULL);
  clock_gettime(CLOCK_MONOTONIC_RAW, &start);
  char* blob = sicm_alloc(device, SICM_LATENCY_SIZE);
  clock_gettime(CLOCK_MONOTONIC_RAW, &end);
  res->alloc = (end.tv_sec - start.tv_sec) * 1000000 + (end.tv_nsec - start.tv_nsec) / 1000;
  clock_gettime(CLOCK_MONOTONIC_RAW, &start);
  for(i = 0; i < SICM_LATENCY_ITERATIONS; i++) {
    sicm_rand(n);
    blob[n % SICM_LATENCY_SIZE] = 0;
  }
  clock_gettime(CLOCK_MONOTONIC_RAW, &end);
  res->write = (end.tv_sec - start.tv_sec) * 1000000 + (end.tv_nsec - start.tv_nsec) / 1000;
  clock_gettime(CLOCK_MONOTONIC_RAW, &start);
  for(i = 0; i < SICM_LATENCY_ITERATIONS; i++) {
    sicm_rand(n);
    b = blob[n % SICM_LATENCY_SIZE];
  }
  clock_gettime(CLOCK_MONOTONIC_RAW, &end);
  // Write it back so hopefully it won't compile away the read
  blob[0] = b;
  res->read = (end.tv_sec - start.tv_sec) * 1000000 + (end.tv_nsec - start.tv_nsec) / 1000;
  clock_gettime(CLOCK_MONOTONIC_RAW, &start);
  sicm_free(device, blob, SICM_LATENCY_SIZE);
  clock_gettime(CLOCK_MONOTONIC_RAW, &end);
  res->free = (end.tv_sec - start.tv_sec) * 1000000 + (end.tv_nsec - start.tv_nsec) / 1000;
}

union size_bytes {
  size_t i;
  char b[sizeof(size_t)];
};

size_t sicm_bandwidth_linear2(struct sicm_device* device, size_t size,
    size_t (*kernel)(double*, double*, size_t)) {
  struct timespec start, end;
  double* a = sicm_alloc(device, size * sizeof(double));
  double* b = sicm_alloc(device, size * sizeof(double));
  unsigned int i;
  #pragma omp parallel for
  for(i = 0; i < size; i++) {
    a[i] = 1;
    b[i] = 2;
  }
  clock_gettime(CLOCK_MONOTONIC_RAW, &start);
  size_t accesses = kernel(a, b, size);
  clock_gettime(CLOCK_MONOTONIC_RAW, &end);
  size_t delta = (end.tv_sec - start.tv_sec) * 1000000 + (end.tv_nsec - start.tv_nsec) / 1000;
  sicm_free(device, a, size * sizeof(double));
  sicm_free(device, b, size * sizeof(double));
  return accesses / delta;
}

size_t sicm_bandwidth_random2(struct sicm_device* device, size_t size,
    size_t (*kernel)(double*, double*, size_t*, size_t)) {
  struct timespec start, end;
  double* a = sicm_alloc(device, size * sizeof(double));
  double* b = sicm_alloc(device, size * sizeof(double));
  size_t* indexes = sicm_alloc(device, size * sizeof(size_t));
  unsigned int i, j;
  union size_bytes bytes;
  #pragma omp parallel for
  for(i = 0; i < size; i++) {
    a[i] = 1;
    b[i] = 2;
    bytes.i = i;
    indexes[i] = 0xcbf29ce484222325;
    for(j = 0; j < sizeof(size_t); j++) {
      indexes[i] ^= bytes.b[j];
      indexes[i] *= 0x100000001b3;
    }
    indexes[i] = indexes[i] % size;
  }
  clock_gettime(CLOCK_MONOTONIC_RAW, &start);
  size_t accesses = kernel(a, b, indexes, size);
  clock_gettime(CLOCK_MONOTONIC_RAW, &end);
  size_t delta = (end.tv_sec - start.tv_sec) * 1000000 + (end.tv_nsec - start.tv_nsec) / 1000;
  sicm_free(device, a, size * sizeof(double));
  sicm_free(device, b, size * sizeof(double));
  sicm_free(device, indexes, size * sizeof(size_t));
  return accesses / delta;
}

size_t sicm_bandwidth_linear3(struct sicm_device* device, size_t size,
    size_t (*kernel)(double*, double*, double*, size_t)) {
  struct timespec start, end;
  double* a = sicm_alloc(device, size * sizeof(double));
  double* b = sicm_alloc(device, size * sizeof(double));
  double* c = sicm_alloc(device, size * sizeof(double));
  unsigned int i;
  #pragma omp parallel for
  for(i = 0; i < size; i++) {
    a[i] = 1;
    b[i] = 2;
    c[i] = 3;
  }
  clock_gettime(CLOCK_MONOTONIC_RAW, &start);
  size_t accesses = kernel(a, b, c, size);
  clock_gettime(CLOCK_MONOTONIC_RAW, &end);
  size_t delta = (end.tv_sec - start.tv_sec) * 1000000 + (end.tv_nsec - start.tv_nsec) / 1000;
  sicm_free(device, a, size * sizeof(double));
  sicm_free(device, b, size * sizeof(double));
  sicm_free(device, c, size * sizeof(double));
  return accesses / delta;
}

size_t sicm_bandwidth_random3(struct sicm_device* device, size_t size,
    size_t (*kernel)(double*, double*, double*, size_t*, size_t)) {
  struct timespec start, end;
  double* a = sicm_alloc(device, size * sizeof(double));
  double* b = sicm_alloc(device, size * sizeof(double));
  double* c = sicm_alloc(device, size * sizeof(double));
  size_t* indexes = sicm_alloc(device, size * sizeof(size_t));
  unsigned int i, j;
  union size_bytes bytes;
  #pragma omp parallel for
  for(i = 0; i < size; i++) {
    a[i] = 1;
    b[i] = 2;
    c[i] = 3;
    bytes.i = i;
    indexes[i] = 0xcbf29ce484222325;
    for(j = 0; j < sizeof(size_t); j++) {
      indexes[i] ^= bytes.b[j];
      indexes[i] *= 0x100000001b3;
    }
    indexes[i] = indexes[i] % size;
  }
  clock_gettime(CLOCK_MONOTONIC_RAW, &start);
  size_t accesses = kernel(a, b, c, indexes, size);
  clock_gettime(CLOCK_MONOTONIC_RAW, &end);
  size_t delta = (end.tv_sec - start.tv_sec) * 1000000 + (end.tv_nsec - start.tv_nsec) / 1000;
  sicm_free(device, a, size * sizeof(double));
  sicm_free(device, b, size * sizeof(double));
  sicm_free(device, c, size * sizeof(double));
  sicm_free(device, indexes, size * sizeof(size_t));
  return accesses / delta;
}

int sicm_move(struct sicm_device* src, struct sicm_device* dest, void* ptr, size_t size) {
  if(src->move_ty == SICM_MOVER_NUMA && dest->move_ty == SICM_MOVER_NUMA) {
    int dest_node = dest->move_payload.numa;
    nodemask_t nodemask;
    nodemask_zero(&nodemask);
    nodemask_set_compat(&nodemask, dest_node);
    return mbind(ptr, size, MPOL_BIND, nodemask.n, numa_max_node() + 2, MPOL_MF_MOVE);
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

struct sicm_device_list sicm_init() {
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
  
  return (struct sicm_device_list){ .count=spec_count, .devices=devices };
}

int main() {
  struct sicm_device_list state = sicm_init();
  struct sicm_device* devices = state.devices;
  int i;
  
  /*
   * test code starts here
   * everything above this comment is required spinup
   */
  printf("%lu\n", sizeof(size_t));
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
  /*int samples = 5;
  double bw = 0;
  for(i = 0; i < samples; i++) bw += sicm_bandwidth(&devices[0]);*/
  size_t best = 0;
  for(i = 0; i < 10; i++) {
    size_t res = sicm_bandwidth_random3(&devices[0], 10000000, sicm_triad_kernel_random);
    if(res > best) best = res;
  }
  printf("bw: %lu\n", best);
  /*char path[100];
  sprintf(path, "cat /proc/%d/numa_maps", (int)getpid());
  system(path);
  sicm_move(&devices[0], &devices[1], test, count * sizeof(int));
  system(path);
  sicm_free(&devices[1], test, count * sizeof(int));*/
  return 1;
}
