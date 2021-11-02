#include "detect_devices/powerpc.h"

void detect_powerpc(struct bitmask* compute_nodes, struct bitmask* non_dram_nodes,
                    int *huge_page_sizes, int huge_page_size_count, int normal_page_size,
                    struct sicm_device **devices, int *curr_idx) {
  #ifdef __powerpc__
  int idx = *curr_idx;

  // Power PC
  for(int i = 0; i <= numa_max_node(); i++) {
    if(!numa_bitmask_isbitset(compute_nodes, i)) {
      // make sure the numa node has memory on it
      long size = -1;
      if ((numa_node_size(i, &size) != -1) && size) {
        devices[idx]->tag = SICM_POWERPC_HBM;
        devices[idx]->node = i;
        devices[idx]->page_size = normal_page_size;
        devices[idx]->data.powerpc_hbm = (struct sicm_powerpc_hbm_data){ };
        numa_bitmask_setbit(non_dram_nodes, i);
        idx++;
        for(j = 0; j < huge_page_size_count; j++) {
          devices[idx]->tag = SICM_POWERPC_HBM;
          devices[idx]->node = i;
          devices[idx]->page_size = huge_page_sizes[j];
          devices[idx]->data.powerpc_hbm = (struct sicm_powerpc_hbm_data){ };
          idx++;
        }
      }
    }
  }

  *curr_idx = idx;
  #endif
}
