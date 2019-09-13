#pragma once

#include "sicm_tree.h"


/* Node Kinds */
#define LAYOUT_NODE_UNKNOWN      (0x0)
#define LAYOUT_NODE_MEM          (0x1)
#define LAYOUT_NODE_COMPUTE      (0x2)

/* Node Attributes */
#define LAYOUT_NODE_NEAR_NIC     (0x0)
#define LAYOUT_NODE_HBM          (0x1)
#define LAYOUT_NODE_NVM          (0x2)
#define LAYOUT_NODE_GPU          (0x4)

#define LAYOUT_NODE_LINE_UNKNOWN (-1)
#define LAYOUT_NODE_NUMA_UNKNOWN (-1L)
#define LAYOUT_NODE_CAP_UNKNOWN  (-1L)

/* Edge Attributes */
#define LAYOUT_EDGE_BW_UNKNOWN   (-1L)
#define LAYOUT_EDGE_LAT_UNKNOWN  (-1L)

typedef struct {
    long int bw;
    long int lat;
} sicm_layout_edge_t, *sicm_layout_edge_ptr;

typedef const char *sicm_layout_str;
use_tree(sicm_layout_str, sicm_layout_edge_ptr);

typedef struct {
    const char                 *name;
    int                         line;
    long int                    numa_node_id;
    long int                    kind;
    int                         attrs;
    long int                    capacity;
    tree(sicm_layout_str,
         sicm_layout_edge_ptr)  edges;
} sicm_layout_node_t, *sicm_layout_node_ptr;

use_tree(sicm_layout_str, sicm_layout_node_ptr);

typedef struct {
    const char                  *name,
                                *path;
    tree(sicm_layout_str,
         sicm_layout_node_ptr)   nodes;
    const char                 **flat_nodes;
    int                          is_valid;
} sicm_layout_t;



/*
 * The public interface.
 */
typedef const char *sicm_layout_node_handle;

void sicm_layout_init(const char *layout_file);
void sicm_layout_fini(void);

int sicm_layout_num_nodes();
sicm_layout_node_handle * sicm_layout_nodes();
const char * sicm_layout_node_name(sicm_layout_node_handle handle);
int sicm_layout_node_kind(sicm_layout_node_handle handle);
long int * sicm_layout_node_numa(sicm_layout_node_handle handle);
long int sicm_layout_node_capacity(sicm_layout_node_handle handle);
int sicm_layout_node_is_near_nic(sicm_layout_node_handle handle);
int sicm_layout_node_is_hbm(sicm_layout_node_handle handle);
int sicm_layout_node_is_nvm(sicm_layout_node_handle handle);
int sicm_layout_node_is_gpu(sicm_layout_node_handle handle);
