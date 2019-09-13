#pragma once

#include "sicm_tree.h"


/* Node Kinds */
#define SL_NODE_UNKNOWN      (0x0)
#define SL_NODE_MEM          (0x1)
#define SL_NODE_COMPUTE      (0x2)

/* Node Attributes */
#define SL_NODE_NEAR_NIC     (0x0)
#define SL_NODE_HBM          (0x1)
#define SL_NODE_NVM          (0x2)
#define SL_NODE_GPU          (0x4)

#define SL_NODE_LINE_UNKNOWN (-1)
#define SL_NODE_NUMA_UNKNOWN (-1L)
#define SL_NODE_CAP_UNKNOWN  (-1L)

/* Edge Attributes */
#define SL_EDGE_BW_UNKNOWN   (-1L)
#define SL_EDGE_LAT_UNKNOWN  (-1L)

typedef struct {
    long int bw;
    long int lat;
} sl_edge_t, *sl_edge_ptr;

typedef const char *sl_str;
use_tree(sl_str, sl_edge_ptr);

typedef struct {
    const char                *name;
    int                        line;
    long int                   numa_node_id;
    long int                   kind;
    int                        attrs;
    long int                   capacity;
    tree(sl_str, sl_edge_ptr)  edges;
} sl_node_t, *sl_node_ptr;

use_tree(sl_str, sl_node_ptr);

typedef struct {
    const char                 *name,
                               *path;
    tree(sl_str, sl_node_ptr)   nodes;
    const char                **flat_nodes;
    int                         is_valid;
} sl_t;


/*
 * The public interface.
 */
typedef const char *sl_node_handle;
typedef sl_edge_ptr sl_edge_handle;

#define SL_NO_EDGE (NULL)

void             sl_init(const char *layout_file);
void             sl_fini(void);

int              sl_num_nodes();
sl_node_handle * sl_nodes();
const char *     sl_node_name(sl_node_handle handle);
int              sl_node_kind(sl_node_handle handle);
long int *       sl_node_numa(sl_node_handle handle);
long int         sl_node_capacity(sl_node_handle handle);
int              sl_node_is_near_nic(sl_node_handle handle);
int              sl_node_is_hbm(sl_node_handle handle);
int              sl_node_is_nvm(sl_node_handle handle);
int              sl_node_is_gpu(sl_node_handle handle);

sl_edge_handle   sl_edge(sl_node_handle src, sl_node_handle dst);
