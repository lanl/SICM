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
 */

int main(int argc, char **argv) {
    const char     *layout_path;
    sl_node_handle *nodes,
                    node,
                    high_bw_node;
    sl_edge_handle  edge;
    int             num_nodes,
                    i,
                    j;
    long            total_cap,
                    cap,
                    max_bw,
                    bw;

    layout_path  = NULL;
    total_cap    = 0;
    high_bw_node = SL_NULL_NODE;

    if (argc > 1) {
        layout_path = argv[1];
    }

    sl_init(layout_path);

    num_nodes = sl_num_nodes();
    nodes     = sl_nodes();

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

    printf("total gpu hbm capacity: %ld GB\n", total_cap);

    for (i = 0; i < num_nodes; i += 1) {
        node = nodes[i];
        if (sl_node_kind(node) == SL_NODE_COMPUTE) {
            max_bw = 0;
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
                printf("node %s has no edges to memory\n", sl_node_name(node));
            } else {
                printf("%s -> %s @ %ld GB/s\n", sl_node_name(node), sl_node_name(high_bw_node), max_bw);
            }
        }
    }

    for (i = 0; i < num_nodes; i += 1) {
        node = nodes[i];
        if (sl_node_is_near_nic(node)) {
            printf("%s is near the NIC\n", sl_node_name(node));
        }
    }

    sl_fini();

    return 0;
}
