#include <numa.h>

#include "detect_devices.h"

typedef int (* node_mod_t)(void);

static const node_mod_t node_mods[] = {
    get_HIP_node_count,
};

static const size_t node_mod_count = sizeof(node_mods) / sizeof(node_mod_t);

int get_node_count(void) {
    int node_count = numa_max_node() + 1;

    for(size_t i = 0; i < node_mod_count; i++) {
        node_count += node_mods[i]();
    }

    return node_count;
}

static struct bitmask *get_compute_nodes(int node_count) {
    struct bitmask* compute_nodes = numa_bitmask_alloc(node_count);
    struct bitmask* cpumask = numa_allocate_cpumask();
    int cpu_count = numa_num_possible_cpus();
    for(int i = 0; i < node_count; i++) {
        numa_node_to_cpus(i, cpumask);
        for(int j = 0; j < cpu_count; j++) {
            if(numa_bitmask_isbitset(cpumask, j)) {
                numa_bitmask_setbit(compute_nodes, i);
                break;
            }
        }
    }
    numa_free_cpumask(cpumask);
    return compute_nodes;
}

typedef void (* detector_func_t)(struct bitmask* compute_nodes, struct bitmask* non_dram_nodes,
                                 int *huge_page_sizes, int huge_page_size_count, int normal_page_size,
                                 struct sicm_device **devices, int *curr_idx);

static const detector_func_t detectors[] = {
    detect_x86,
    detect_powerpc,
    detect_HIP,
    detect_DRAM,
};

static const size_t detector_count = sizeof(detectors) / sizeof(detector_func_t);

/* fill in device list */
int detect_devices(int node_count, int *huge_page_sizes,
                   int huge_page_size_count, int normal_page_size,
                   struct sicm_device **devices) {
    struct bitmask* compute_nodes = get_compute_nodes(node_count);
    struct bitmask* non_dram_nodes = numa_bitmask_alloc(node_count);

    int idx = 0;
    for(size_t i = 0; i < detector_count; i++) {
        detectors[i](compute_nodes, non_dram_nodes,
                     huge_page_sizes, huge_page_size_count, normal_page_size,
                     devices, &idx);
    }

    numa_bitmask_free(non_dram_nodes);
    numa_bitmask_free(compute_nodes);

    return idx;
}
