#include <stdio.h>

#include "sicm_layout.h"

/*
 * An example program that uses sicm-layout to find the combined
 * capacity of all GPU HBM on the system.
 *
 * Then, for each compute node, it finds the memory node connected
 * to it with the highest bandwidth.
 *
 * Then, it lists each node that has the 'near_nic' attribute set.
 *
 * Finally, it lists the sicm_device pointer for each node that can
 * be used with the rest of the SICM API.
 */

void gpu_hbm(sl_node_handle *nodes, int num_nodes);
void high_bw(sl_node_handle *nodes, int num_nodes);
void near_nic(sl_node_handle *nodes, int num_nodes);
void devices(sl_node_handle *nodes, int num_nodes);

int main(int argc, char **argv) {
    const char     *layout_path;
    sl_node_handle *nodes;
    int             num_nodes;

    layout_path  = NULL;

    if (argc > 1) {
        layout_path = argv[1];
    }

    sl_init(layout_path);

    printf("the layout is '%s'\n", sl_layout_name());

    num_nodes = sl_num_nodes();
    nodes     = sl_nodes();

    gpu_hbm(nodes, num_nodes);
    high_bw(nodes, num_nodes);
    near_nic(nodes, num_nodes);
    devices(nodes, num_nodes);

    sl_fini();

    return 0;
}

void gpu_hbm(sl_node_handle *nodes, int num_nodes) {
    int            i;
    long           cap, total_cap;
    sl_node_handle node;

    printf("test: gpu_hbm\n");

    total_cap = 0;

    for (i = 0; i < num_nodes; i += 1) {
        node = nodes[i];
        if (sl_node_kind(node) == SL_NODE_MEM) {
            if (sl_node_is_gpu(node) && sl_node_is_hbm(node)) {
                cap = sl_node_capacity(node);
                if (cap != SL_NODE_CAP_UNKNOWN) {
                    total_cap += cap;
                }
            }
        }
    }

    printf("    total gpu hbm capacity: %ld GB\n", total_cap);
}

void high_bw(sl_node_handle *nodes, int num_nodes) {
    int            i, j;
    long           bw, max_bw;
    sl_node_handle node, high_bw_node;
    sl_edge_handle edge;

    printf("test: high_bw\n");

    for (i = 0; i < num_nodes; i += 1) {
        node = nodes[i];

        if (sl_node_kind(node) == SL_NODE_COMPUTE) {
            max_bw = 0;
            high_bw_node = SL_NULL_NODE;

            for (j = 0; j < num_nodes; j += 1) {
                if (sl_node_kind(nodes[j]) == SL_NODE_MEM) {
                    edge = sl_edge(node, nodes[j]);
                    if (edge != SL_NO_EDGE) {
                        bw = sl_edge_bandwidth(edge);
                        if (bw > max_bw) {
                            max_bw       = bw;
                            high_bw_node = sl_edge_dst(edge);
                        }
                    }
                }
            }

            if (high_bw_node == SL_NULL_NODE) {
                printf("    node %s has no edges to memory\n", sl_node_name(node));
            } else {
                printf("    %s -> %s @ %ld GB/s\n", sl_node_name(node), sl_node_name(high_bw_node), max_bw);
            }
        }
    }
}

void near_nic(sl_node_handle *nodes, int num_nodes) {
    int i;

    printf("test: near_nic\n");

    for (i = 0; i < num_nodes; i += 1) {
        if (sl_node_is_near_nic(nodes[i])) {
            printf("    %s is near the NIC\n", sl_node_name(nodes[i]));
        }
    }
}

void devices(sl_node_handle *nodes, int num_nodes) {
    int i;

    printf("test: devices\n");

    for (i = 0; i < num_nodes; i += 1) {
        printf("    device for '%s': %p\n", sl_node_name(nodes[i]), sl_node_device(nodes[i]));
    }
}
