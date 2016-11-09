#pragma once
#include <stdlib.h>
#include <numa.h>

#define SICM_BLOB_SIZE 4

struct sicm_device {
  char payload[SICM_BLOB_SIZE];
  void* (*alloc)(struct sicm_device*, size_t);
  void (*free)(struct sicm_device*, void*, size_t);
  int (*add_to_bitmask)(struct sicm_device*, struct bitmask*);
};

struct sicm_device_spec {
  int (*non_numa_count)();
  int (*add_devices)(struct sicm_device*, int, struct bitmask*);
};

int zero();

void* sicm_alloc(struct sicm_device* device, size_t size);
void sicm_free(struct sicm_device* device, void* ptr, size_t size);
int sicm_add_to_bitmask(struct sicm_device* device, struct bitmask* mask);
