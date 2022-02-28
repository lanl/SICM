#ifndef __SICM_DETECT_HIP_H
#define __SICM_DETECT_HIP_H

#include "detect_devices.h"

int get_HIP_node_count();

void detect_HIP(struct bitmask* compute_nodes, struct bitmask* non_dram_nodes,
                int *huge_page_sizes, int huge_page_size_count, int normal_page_size,
                struct sicm_device **devices, int *curr_idx);

#endif
