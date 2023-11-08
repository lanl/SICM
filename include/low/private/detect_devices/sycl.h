#ifndef __SICM_DETECT_SYCL_H
#define __SICM_DETECT_SYCL_H

#include "detect_devices.h"

#ifdef __cplusplus
extern "C" {
#endif
int get_sycl_node_count();

void detect_sycl(struct bitmask* compute_nodes, struct bitmask* non_dram_nodes,
                 int *huge_page_sizes, int huge_page_size_count, int normal_page_size,
                 struct sicm_device **devices, int *curr_idx);

void *sicm_alloc_sycl_device(struct sicm_device *device, size_t size);

#ifdef __cplusplus
}
#endif

#endif
