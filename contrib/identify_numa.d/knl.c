#include <numa.h>
#include <stdint.h>
#include <stdio.h>

#define X86_CPUID_MODEL_MASK        (0xf<<4)
#define X86_CPUID_EXT_MODEL_MASK    (0xf<<16)

int main() {
#ifdef __x86_64__
    const int node_count = numa_max_node() + 1;

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
        struct bitmask* cpumask = numa_allocate_cpumask();
        int cpu_count = numa_num_possible_cpus();
        struct bitmask* compute_nodes = numa_bitmask_alloc(node_count);
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

        for(int i = 0; i < node_count; i++) {
            if(!numa_bitmask_isbitset(compute_nodes, i)) {
                long size = -1;
                if ((numa_node_size(i, &size) != -1) && size) {
                    printf("%d KNL\n", i);
                }
            }
        }
    }
#endif

    return 0;
}
