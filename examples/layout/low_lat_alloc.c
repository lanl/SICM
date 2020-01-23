#include "sicm_low.h"
#include "sicm_layout.h"

#include <utmpx.h> /* sched_getcpu     */
#include <numa.h>  /* numa_node_of_cpu */

#include <stdlib.h>
#include <string.h>
#include <stdio.h>

static sl_node_handle cpu_node;

sl_node_handle find_cpu_node(sl_node_handle *nodes, int n_nodes);
int lat_cmp(const void *a, const void *b);
sl_node_handle *lat_sort(sl_node_handle *node, size_t n);
void * node_alloc(sl_node_handle node, size_t n);

int main(int argc, char **argv) {
    int             n_nodes;
    sl_node_handle *nodes,
                   *nodes_sorted_by_latency;
    void           *my_mem;

    sl_init(NULL);

    n_nodes  = sl_num_nodes();
    nodes    = sl_nodes();
    cpu_node = find_cpu_node(nodes, n_nodes);

    nodes_sorted_by_latency = lat_sort(nodes, n_nodes);

    my_mem = node_alloc(nodes_sorted_by_latency[0], 12345);

    /* Do something with the low-latency memory. */

    sl_fini();

    return 0;
}

/* Find the sl_node that refers to the CPU this code is running on. */
sl_node_handle find_cpu_node(sl_node_handle *nodes, int n_nodes) {
    int cpu_node_id,
        i;
    sl_node_handle node;

    cpu_node_id = numa_node_of_cpu(sched_getcpu());

    for (i = 0; i < n_nodes; i += 1) {
        node = nodes[i];
        if (sl_node_kind(node) == SL_NODE_COMPUTE
        &&  sl_node_numa(node) == cpu_node_id) {
            return node;
        }
    }
}

/* Compare the latency of different memory nodes from this CPU. */
int lat_cmp(const void *a, const void *b) {
    sl_edge_handle cpu_to_a,
                   cpu_to_b;
    long int       lat_a,
                   lat_b;

    cpu_to_a = sl_edge(cpu_node, *(sl_node_handle*)a);
    lat_a    = cpu_to_a ? sl_edge_latency(cpu_to_a) : -1;
    if (lat_a == -1 || sl_node_kind(*(sl_node_handle*)a) != SL_NODE_MEM) {
        lat_a = INT64_MAX;
    }

    cpu_to_b = sl_edge(cpu_node, *(sl_node_handle*)b);
    lat_b    = cpu_to_b ? sl_edge_latency(cpu_to_b) : -1;
    if (lat_b == -1 || sl_node_kind(*(sl_node_handle*)b) != SL_NODE_MEM) {
        lat_b = INT64_MAX;
    }

    return lat_a - lat_b;
}

sl_node_handle *lat_sort(sl_node_handle *nodes, size_t n) {
    int             i;
    sl_node_handle *sorted;

    sorted = malloc(sizeof(*sorted) * n);
    memcpy(sorted, nodes, sizeof(*sorted) * n);

    qsort(sorted, n, sizeof(*sorted), lat_cmp);

    return sorted;
}

/* Use an sl_node to allocate with the SICM low-level interface. */
void * node_alloc(sl_node_handle node, size_t n) {
    sicm_device *d;

    d = sl_node_device(node);

    if (d == NULL) {
        return NULL;
    }

    return sicm_device_alloc(d, n);
}
