#include "detect_devices/x86.h"

#define X86_CPUID_MODEL_MASK        (0xf<<4)
#define X86_CPUID_EXT_MODEL_MASK    (0xf<<16)

void detect_x86(struct bitmask* compute_nodes, struct bitmask* non_dram_nodes,
                int *huge_page_sizes, int huge_page_size_count, int normal_page_size,
                struct sicm_device **devices, int *curr_idx) {
  #ifdef __x86_64__
  int idx = *curr_idx;

  int i, j;

  // Knights Landing
  uint32_t xeon_phi_model = (0x7<<4);
  uint32_t xeon_phi_ext_model = (0x5<<16);
  uint32_t registers[4];
  uint32_t expected = xeon_phi_model | xeon_phi_ext_model;
  asm volatile("cpuid":"=a"(registers[0]),
                       "=b"(registers[1]),
                       "=c"(registers[2]),
                       "=d"(registers[2]):"0"(1), "2"(0));
  uint32_t actual = registers[0] & (X86_CPUID_MODEL_MASK | X86_CPUID_EXT_MODEL_MASK);

  if (actual == expected) {
    for(i = 0; i <= numa_max_node(); i++) {
      if(!numa_bitmask_isbitset(compute_nodes, i)) {
        long size = -1;
        if ((numa_node_size(i, &size) != -1) && size) {
          int compute_node = -1;
          /*
           * On Knights Landing machines, high-bandwidth memory always has
           * higher NUMA distances (to prevent malloc from giving you HBM)
           * but I'm pretty sure the compute node closest to an HBM node
           * always has NUMA distance 31, e.g.,
           * https://goparallel.sourceforge.net/wp-content/uploads/2016/05/Colfax_KNL_Clustering_Modes_Guide.pdf
           */
          for(j = 0; j < numa_max_node(); j++) {
              if(numa_distance(i, j) == 31) compute_node = j;
          }
          devices[idx]->tag = SICM_KNL_HBM;
          devices[idx]->node = i;
          devices[idx]->page_size = normal_page_size;
          devices[idx]->data.knl_hbm = (struct sicm_knl_hbm_data){
            .compute_node=compute_node };
          numa_bitmask_setbit(non_dram_nodes, i);
          idx++;
          for(j = 0; j < huge_page_size_count; j++) {
              devices[idx]->tag = SICM_KNL_HBM;
              devices[idx]->node = i;
              devices[idx]->page_size = huge_page_sizes[j];
              devices[idx]->data.knl_hbm = (struct sicm_knl_hbm_data){
                .compute_node=compute_node };
              idx++;
          }
        }
      }
    }
 } else {
   // Optane support
   // This is a bit of a hack: on x86_64 architecture that is not KNL,
   // NUMA nodes without CPUs are assumed to be Optane nodes
   for(i = 0; i <= numa_max_node(); i++) {
     if(!numa_bitmask_isbitset(compute_nodes, i)) {
       long size = -1;
       if ((numa_node_size(i, &size) != -1) && size) {
         int compute_node = -1;
	 int dist = 1000;
         for(j = 0; j < numa_max_node(); j++) {
	   if (i == j)
	     continue;
	   int d = numa_distance(i, j);
	   if (d < dist) {
		dist = d;
		compute_node = j;
	   }
         }
         devices[idx]->tag = SICM_OPTANE;
         devices[idx]->node = i;
         devices[idx]->page_size = normal_page_size;
         devices[idx]->data.optane = (struct sicm_optane_data){
           .compute_node=compute_node };
         numa_bitmask_setbit(non_dram_nodes, i);
         idx++;
         for(j = 0; j < huge_page_size_count; j++) {
             devices[idx]->tag = SICM_OPTANE;
             devices[idx]->node = i;
             devices[idx]->page_size = huge_page_sizes[j];
             devices[idx]->data.optane = (struct sicm_optane_data){
               .compute_node=compute_node };
             idx++;
         }
       }
     }
   }
  }

  *curr_idx = idx;
  #endif
}
