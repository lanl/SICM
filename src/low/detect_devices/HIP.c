#include "detect_devices/HIP.h"
#include <stdio.h>

#include <hip/hip_runtime.h>

int get_HIP_node_count() {
    int usable = 0;

    hipInit(0);

    int dev_count = 0;
    hipGetDeviceCount(&dev_count);

    for(int i = 0; i < dev_count; i++) {
        hipDeviceProp_t prop;
        hipGetDeviceProperties(&prop, i);

        if (!prop.canMapHostMemory ||
            !prop.totalGlobalMem) {
            continue;
        }

        usable++;
    }

    return usable;
}

static int get_numa_node(int pciBusID) {
    int numa_node = -1;
    char filename[PATH_MAX];
    snprintf(filename, PATH_MAX, "/sys/class/pci_bus/0000:%02x/device/numa_node", pciBusID);
    FILE *f = fopen(filename, "r");
    if (f) {
        // ignore errors
        fscanf(f, "%d", &numa_node);
    }
    fclose(f);
    return numa_node;
}

void detect_HIP(struct bitmask* compute_nodes, struct bitmask* non_dram_nodes,
                int *huge_page_sizes, int huge_page_size_count, int normal_page_size,
                struct sicm_device **devices, int *curr_idx) {
    int idx = *curr_idx;

    int dev_count = 0;
    hipGetDeviceCount(&dev_count);

    for(int i = 0; i < dev_count; i++) {
        hipDeviceProp_t prop;
        hipGetDeviceProperties(&prop, i);

        if (!prop.canMapHostMemory ||
            !prop.totalGlobalMem) {
            continue;
        }

        const int numa_node = get_numa_node(prop.pciBusID);
        const int compute_node = numa_bitmask_isbitset(compute_nodes, numa_node);

        devices[idx]->tag = SICM_HIP;
        devices[idx]->node = numa_node;
        devices[idx]->page_size = normal_page_size;
        devices[idx]->data.hip = (struct sicm_hip_data){ .compute_node = compute_node, .id = i };
        // do not modify non_dram_nodes since HIP devices might be co-located with DRAM
        idx++;
        for(int j = 0; j < huge_page_size_count; j++) {
            devices[idx]->tag = SICM_HIP;
            devices[idx]->node = numa_node;
            devices[idx]->page_size = huge_page_sizes[j];
            devices[idx]->data.hip = (struct sicm_hip_data){ .compute_node = compute_node, .id = i };
            idx++;
        }
    }

    *curr_idx = idx;
}
