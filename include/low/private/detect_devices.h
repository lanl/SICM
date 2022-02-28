#ifndef __SICM_DETECT_DEVICES_H
#define __SICM_DETECT_DEVICES_H

#include <numa.h>

#include "sicm_low.h"

int get_node_count();

int detect_devices(int node_count,
                   int *huge_page_sizes, int huge_page_size_count, int normal_page_size,
                   struct sicm_device **devices);

/* signature to add more nodes to node count */
typedef int (* node_mod_t)(void);

/* signature for detectors to implement to fill in devices list  */
typedef void (* detector_func_t)(struct bitmask* compute_nodes, struct bitmask* non_dram_nodes,
                                 int *huge_page_sizes, int huge_page_size_count, int normal_page_size,
                                 struct sicm_device **devices, int *curr_idx);


#endif
