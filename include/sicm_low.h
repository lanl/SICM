#pragma once
#include <errno.h>
#include <fcntl.h>
#include <numa.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <sys/mman.h>
#include <unistd.h>

#define SICM_BLOB_SIZE 4

enum sicm_device_type { SICM_KNL_HBM, SICM_DRAM };
enum sicm_mover_type { SICM_MOVER_NUMA };
union sicm_mover_payload {
  int numa;
};

struct sicm_device {
  char payload[SICM_BLOB_SIZE];
  enum sicm_device_type ty;
  enum sicm_mover_type move_ty;
  union sicm_mover_payload move_payload;
  void* (*alloc)(struct sicm_device*, size_t);
  void (*free)(struct sicm_device*, void*, size_t);
  size_t (*used)(struct sicm_device*);
  size_t (*capacity)(struct sicm_device*);
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
int sicm_add_to_bitmask(struct sicm_device*, struct bitmask*);
int sicm_move(struct sicm_device*, struct sicm_device*, void*, size_t);
