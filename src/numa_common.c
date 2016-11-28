/*
 * Defines generic implementations for low-level functions on NUMA
 * nodes. The big expectation is that the first four bytes of the
 * payload are an integer containing the NUMA node number.
 */
#include "numa_common.h"

void* sicm_numa_common_alloc(struct sicm_device* device, size_t size) {
  return numa_alloc_onnode(size, device->move_payload.numa);
}

void sicm_numa_common_free(struct sicm_device* device, void* start, size_t size) {
  numa_free(start, size);
}

size_t sicm_numa_common_used(struct sicm_device* device) {
  int node = device->move_payload.numa;
  char path[50];
  sprintf(path, "/sys/devices/system/node/node%d/meminfo", node);
  int fd = open(path, O_RDONLY);
  char data[101];
  read(fd, data, 101);
  close(fd);
  size_t res = 0;
  size_t factor = 1;
  int i;
  for(i = 100; data[i] != ' '; i--) {
    res += factor * (data[i] - '0');
    factor *= 10;
  }
  return res * 1024;
}

size_t sicm_numa_common_capacity(struct sicm_device* device) {
  int node = device->move_payload.numa;
  char path[50];
  sprintf(path, "/sys/devices/system/node/node%d/meminfo", node);
  int fd = open(path, O_RDONLY);
  char data[31];
  read(fd, data, 31);
  close(fd);
  size_t res = 0;
  size_t factor = 1;
  int i;
  for(i = 30; data[i] != ' '; i--) {
    res += factor * (data[i] - '0');
    factor *= 10;
  }
  return res * 1024;
}

int sicm_numa_common_model_distance(struct sicm_device* device) {
  return numa_distance(device->move_payload.numa, numa_node_of_cpu(sched_getcpu()));
}

void sicm_numa_common_latency(struct sicm_device* device, struct sicm_timing* res) {
  struct timespec start, end;
  int i;
  char b;
  clock_gettime(CLOCK_MONOTONIC_RAW, &start);
  char* blob = sicm_alloc(device, TIMING_ITERATIONS);
  clock_gettime(CLOCK_MONOTONIC_RAW, &end);
  res->alloc = (end.tv_sec - start.tv_sec) * 1000000 + (end.tv_nsec - start.tv_nsec) / 1000;
  clock_gettime(CLOCK_MONOTONIC_RAW, &start);
  for(i = 0; i < TIMING_ITERATIONS; i++) blob[i] = 0;
  clock_gettime(CLOCK_MONOTONIC_RAW, &end);
  res->write = (end.tv_sec - start.tv_sec) * 1000000 + (end.tv_nsec - start.tv_nsec) / 1000;
  clock_gettime(CLOCK_MONOTONIC_RAW, &start);
  for(i = 0; i < TIMING_ITERATIONS; i++) b = blob[i];
  clock_gettime(CLOCK_MONOTONIC_RAW, &end);
  // Write it back so hopefully it won't compile away the read
  blob[0] = b;
  res->read = (end.tv_sec - start.tv_sec) * 1000000 + (end.tv_nsec - start.tv_nsec) / 1000;
  clock_gettime(CLOCK_MONOTONIC_RAW, &start);
  sicm_free(device, blob, TIMING_ITERATIONS);
  clock_gettime(CLOCK_MONOTONIC_RAW, &end);
  res->free = (end.tv_sec - start.tv_sec) * 1000000 + (end.tv_nsec - start.tv_nsec) / 1000;
}

int sicm_numa_common_add_to_bitmask(struct sicm_device* device, struct bitmask* mask) {
  numa_bitmask_setbit(mask, device->move_payload.numa);
  return 1;
}
