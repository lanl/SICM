#include "sicm_low.h"
#include <stdio.h>

#define test_kib 102400
#define bw_samples 10

int main() {
  struct sicm_device_list devices = sicm_init();
  printf("device count: %d\n\n", devices.count);
  
  int i;
  unsigned int j;
  for(i = 0; i < devices.count; i++) {
    struct sicm_device* device = &devices.devices[i];
    printf("device %d\n", i);
    printf("type: ");
    switch(device->tag) {
      case SICM_DRAM:
        printf("dram\n");
        break;
      case SICM_KNL_HBM:
        printf("knl hbm\n");
        break;
      default:
        printf("unknown (update this example!)\n");
    }
    printf("numa node: %d\n", sicm_numa_id(device));
    printf("page size: %d\n", sicm_device_page_size(device));
    printf("capacity: %lu\n", sicm_capacity(device));
    printf("available: %lu\n", sicm_avail(device));
    
    if(sicm_avail(device) >= test_kib) {
      printf("verifying alloc\n");
      unsigned int* blob = sicm_alloc(device, test_kib * 1024);
      printf("verifying write\n");
      for(j = 0; j < test_kib * 1024 / sizeof(int); j++) blob[j] = sicm_hash(j);
      printf("verifying free\n");
      sicm_free(device, blob, test_kib * 1024);
      struct sicm_timing timing;
      sicm_latency(device, test_kib * 1024, 1000, &timing);
      printf("latency timings:\n");
      printf("  alloc: %d\n", timing.alloc);
      printf("  write: %d\n", timing.write);
      printf("  read: %d\n", timing.read);
      printf("  free: %d\n", timing.free);
      size_t best_bw = 0;
      for(j = 0; j < bw_samples; j++) {
        size_t bw = sicm_bandwidth_linear3(device, test_kib * 1024 / 24, sicm_triad_kernel_linear);
        if(bw > best_bw) best_bw = bw;
      }
      printf("bandwidth: %luMiB/s\n", best_bw);
    }
    else {
      printf("not enough space to do timings\n");
    }
    printf("\n");
  }
  
  return 1;
}
