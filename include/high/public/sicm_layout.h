#pragma once

/* Node Kinds */
#define SL_NODE_UNKNOWN      (0x0)
#define SL_NODE_MEM          (0x1)
#define SL_NODE_COMPUTE      (0x2)

/* Node Attributes */
#define SL_NODE_NEAR_NIC     (0x1)
#define SL_NODE_HBM          (0x2)
#define SL_NODE_NVM          (0x4)
#define SL_NODE_GPU          (0x8)

#define SL_NODE_LINE_UNKNOWN (-1)
#define SL_NODE_NUMA_UNKNOWN (-1L)
#define SL_NODE_CAP_UNKNOWN  (-1L)

/* Edge Attributes */
#define SL_EDGE_BW_UNKNOWN   (-1L)
#define SL_EDGE_LAT_UNKNOWN  (-1L)

#define SL_NULL_NODE         (NULL)
#define SL_NO_EDGE           (NULL)

typedef const char *sl_node_handle;
struct sl_edge_t;
typedef struct sl_edge_t *sl_edge_handle;

void             sl_init(const char *layout_file);
void             sl_fini(void);

const char *     sl_layout_name(void);
int              sl_num_nodes(void);
sl_node_handle * sl_nodes(void);

const char *     sl_node_name(sl_node_handle handle);
int              sl_node_kind(sl_node_handle handle);
long int         sl_node_numa(sl_node_handle handle);
long int         sl_node_capacity(sl_node_handle handle);
int              sl_node_is_near_nic(sl_node_handle handle);
int              sl_node_is_hbm(sl_node_handle handle);
int              sl_node_is_nvm(sl_node_handle handle);
int              sl_node_is_gpu(sl_node_handle handle);

sl_edge_handle   sl_edge(sl_node_handle src, sl_node_handle dst);
sl_node_handle   sl_edge_src(sl_edge_handle handle);
sl_node_handle   sl_edge_dst(sl_edge_handle handle);
long int         sl_edge_bandwidth(sl_edge_handle handle);
long int         sl_edge_latency(sl_edge_handle handle);

#define sl_node_traverse(it) \
    for ((it) = sl_nodes(); (it) <= sl_nodes() + sl_num_nodes(); (it) += 1)
