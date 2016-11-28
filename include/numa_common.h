#include "sicm_low.h"

void* sicm_numa_common_alloc(struct sicm_device*, size_t);
void sicm_numa_common_free(struct sicm_device*, void*, size_t);
size_t sicm_numa_common_used(struct sicm_device*);
size_t sicm_numa_common_capacity(struct sicm_device*);
int sicm_numa_common_model_distance(struct sicm_device*);
void sicm_numa_common_latency(struct sicm_device*, struct sicm_timing*);
int sicm_numa_common_add_to_bitmask(struct sicm_device*, struct bitmask*);
