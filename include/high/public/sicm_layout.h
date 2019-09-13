#pragma once

#include "sicm_tree.h"


/* Node Kinds */
#define LAYOUT_NODE_UNKNOWN      (0x0)
#define LAYOUT_NODE_MEM          (0x1)
#define LAYOUT_NODE_COMPUTE      (0x2)

/* Node Attributes */
#define LAYOUT_NODE_NEAR_NIC     (0x0)
#define LAYOUT_NODE_LOW_LAT      (0x1)
#define LAYOUT_NODE_HBM          (0x2)
#define LAYOUT_NODE_NVM          (0x4)
#define LAYOUT_NODE_ON_GPU       (0x8)

#define LAYOUT_NODE_LINE_UNKNOWN (-1)
#define LAYOUT_NODE_NUMA_UNKNOWN (-1L)
#define LAYOUT_NODE_CAP_UNKNOWN  (-1L)

/* Edge Attributes */
#define LAYOUT_EDGE_BW_UNKNOWN   (-1L)
#define LATOUT_EDGE_LAT_UNKNOWN  (-1L)

typedef struct {
    long int bw;
    long int lat;
} sicm_layout_edge_t, *sicm_layout_edge_ptr;

typedef const char *str;
use_tree(str, sicm_layout_edge_ptr);

typedef struct {
    const char                      *name;
    int                              line;
    long int                         numa_node_id;
    long int                         kind;
    int                              attrs;
    long int                         capacity;
    tree(str, sicm_layout_edge_ptr)  edges;
} sicm_layout_node_t, *sicm_layout_node_ptr;

use_tree(str, sicm_layout_node_ptr);

typedef struct {
    const char                      *name,
                                    *path;
    tree(str, sicm_layout_node_ptr)  nodes;
} sicm_layout_t;

void sicm_layout_init(const char *layout_file);
void sicm_layout_fini(void);

