#pragma once

#include "sicm_tree.h"


/* Node Kinds */
#define NODE_MEM     (0x0)
#define NODE_COMPUTE (0x1)

/* Node Attributes */
#define NODE_NEAR_NIC    (0x0)
#define NODE_LOW_LAT     (0x1)
#define NODE_HBM         (0x2)
#define NODE_PERSIST     (0x4)
#define NODE_ON_GPU      (0x8)

typedef struct {
    const char *name;
    long int    numa_node_id;
    int         kind;
    int         attrs;
    int         capacity;
} sicm_layout_node_t;

typedef sicm_layout_node_t *sicm_layout_node_ptr;
typedef const char *str;
use_tree(str, sicm_layout_node_ptr);

typedef struct {
    const char                      *name;
    tree(str, sicm_layout_node_ptr)  nodes;
    int                              is_valid;
} sicm_layout_t;

void sicm_layout_init(const char *layout_file);
void sicm_layout_fini(void);

void * sicm_node_alloc(size_t size, const char *node_name);
void * sicm_attr_alloc(size_t size, int attrs);
