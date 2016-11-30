#pragma once
#define _GNU_SOURCE
#include <assert.h>
#include <errno.h>
#include <fcntl.h>
#include <numa.h>
#include <pthread.h>
#include <sched.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <sys/mman.h>
#include <time.h>
#include <unistd.h>

#define SICM_LATENCY_SIZE 4096
#define SICM_LATENCY_ITERATIONS 10000
#define SICM_BANDWIDTH_SIZE 1073741824
#define SICM_BANDWIDTH_THREAD_COUNT 64
#define SICM_BANDWIDTH_COPY_SIZE 1048576
#define SICM_BANDWIDTH_ITERATION_COUNT 1048576

/*
 * This is a linear-feedback shift register PRNG algorithm
 * 
 * This is obviously not a cryptographic algorithm, or even very good
 * for intense random number exercises, but we just want something
 * that'll produce cache misses and avoid prefetching
 * 
 * Also, this algorithm is incredibly fast, so random number generation
 * won't affect timings very much, and it's thread safe
 * 
 * This function must be seeded manually, and n should be unsigned
 */
#define sicm_rand(n) \
  n = (n >> 1) ^ (unsigned int)((0 - (n & 1u)) & 0xd0000001u)

enum sicm_device_type { SICM_KNL_HBM, SICM_DRAM };
enum sicm_mover_type { SICM_MOVER_NUMA };
union sicm_mover_payload {
  int numa;
};

// NOTE: timings are in milliseconds
struct sicm_timing {
  unsigned int alloc;
  unsigned int write;
  unsigned int read;
  unsigned int free;
};

struct sicm_device {
  enum sicm_device_type ty;
  enum sicm_mover_type move_ty;
  union sicm_mover_payload move_payload;
  void* (*alloc)(struct sicm_device*, size_t);
  void (*free)(struct sicm_device*, void*, size_t);
  size_t (*used)(struct sicm_device*);
  size_t (*capacity)(struct sicm_device*);
  int (*model_distance)(struct sicm_device*);
  int (*add_to_bitmask)(struct sicm_device*, struct bitmask*);
};

struct sicm_device_spec {
  int (*non_numa_count)();
  int (*add_devices)(struct sicm_device*, int, struct bitmask*);
};

int zero();

struct bitmask* sicm_cpu_mask();

void* sicm_alloc(struct sicm_device*, size_t);
void sicm_free(struct sicm_device*, void*, size_t);
size_t sicm_used(struct sicm_device*);
int sicm_model_distance(struct sicm_device*);
void sicm_latency(struct sicm_device*, struct sicm_timing*);
double sicm_bandwidth(struct sicm_device*);
int sicm_add_to_bitmask(struct sicm_device*, struct bitmask*);
int sicm_move(struct sicm_device*, struct sicm_device*, void*, size_t);
