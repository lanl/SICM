#include "sg.h"
#include <stdio.h>

int main() {
  sg_init(0);
  int i;
  printf("Performance list:\n");
  for(i = 0; i < sg_performance_list.count; i++) {
    struct sicm_device* device = &sg_performance_list.devices[i];
    if(device->tag == SICM_DRAM) printf("DRAM %d %uKiB\n", device->data.dram.node, device->data.dram.page_size);
    else if(device->tag == SICM_KNL_HBM) printf("KNL HBM %d %uKiB\n", device->data.knl_hbm.node, device->data.knl_hbm.page_size);
    else printf("UNKNOWN\n");
  }

  printf("\nCapacity list:\n");
  for(i = 0; i < sg_capacity_list.count; i++) {
    struct sicm_device* device = &sg_capacity_list.devices[i];
    if(device->tag == SICM_DRAM) printf("DRAM %d %uKiB\n", device->data.dram.node, device->data.dram.page_size);
    else if(device->tag == SICM_KNL_HBM) printf("KNL HBM %d %uKiB\n", device->data.knl_hbm.node, device->data.knl_hbm.page_size);
    else printf("UNKNOWN\n");
  }
}
