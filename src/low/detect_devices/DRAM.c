#include "detect_devices/DRAM.h"

void detect_DRAM(struct bitmask* compute_nodes, struct bitmask* non_dram_nodes,
                 int *huge_page_sizes, int huge_page_size_count, int normal_page_size,
                 struct sicm_device **devices, int *curr_idx) {
  int idx = *curr_idx;

  for(int i = 0; i <= numa_max_node(); i++) {
    if(!numa_bitmask_isbitset(non_dram_nodes, i)) {
      long size = -1;
      if ((numa_node_size(i, &size) != -1) && size) {
        devices[idx]->tag = SICM_DRAM;
        devices[idx]->node = i;
        devices[idx]->page_size = normal_page_size;
        devices[idx]->data.dram = (struct sicm_dram_data){ };
        idx++;
        for(int j = 0; j < huge_page_size_count; j++) {
          devices[idx]->tag = SICM_DRAM;
          devices[idx]->node = i;
          devices[idx]->page_size = huge_page_sizes[j];
          devices[idx]->data.dram = (struct sicm_dram_data){ };
          idx++;
        }
      }
    }
  }

  *curr_idx = idx;
}
