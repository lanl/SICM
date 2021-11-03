#ifndef __SICM_DETECT_POWERPC_H
#define __SICM_DETECT_POWERPC_H

#include <numa.h>
#include <sicm_low.h>

void detect_powerpc(struct bitmask* compute_nodes, struct bitmask* non_dram_nodes,
                    int *huge_page_sizes, int huge_page_size_count, int normal_page_size,
                    struct sicm_device **devices, int *curr_idx);

#endif
