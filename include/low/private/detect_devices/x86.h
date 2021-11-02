#ifndef __SICM_DETECT_X86_H
#define __SICM_DETECT_X86_H

#include <numa.h>
#include <sicm_low.h>

void detect_x86(struct bitmask* compute_nodes, struct bitmask* non_dram_nodes,
                int *huge_page_sizes, int huge_page_size_count, int normal_page_size,
                struct sicm_device **devices, int *curr_idx);

#endif
