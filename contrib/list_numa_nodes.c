#include <stdio.h>
#include <numa.h>

int main() {
    const int node_count = numa_max_node() + 1;
    for(int i = 0; i < node_count; i++) {
        long long freep = 0;
        if (numa_node_size64(i, &freep) != -1) {
            if (freep) {
                printf("%d %lld\n", i, freep);
            }
        }
    }

    return 0;
}
