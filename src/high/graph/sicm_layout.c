#include "sicm_layout.h"

#include <stdio.h>
#include <stdlib.h>

#define ERR(...) do {                                    \
    fprintf(stderr,"[sicm-layout] ERROR: " __VA_ARGS__); \
    exit(1);                                             \
} while (0)

#define LOG(...) do {                               \
    fprintf(stderr, "[sicm-layout]: " __VA_ARGS__); \
} while (0)

static sicm_layout_t layout;

static void parse_layout_file(const char *layout_file) {
    FILE *f;

    f = fopen(layout_file, "w");

    LOG("using layout file '%s'\n", layout_file);

    if (f == NULL) {
        ERR("Could not open layout file '%s'.\n", layout_file);
    }

    layout.nodes = tree_make(str, sicm_layout_node_t);

    /*
     * @incomplete
     */

    layout.is_valid = 1;
}

void sicm_layout_init(const char *layout_file) {
    if (layout_file == NULL) { layout_file = getenv("SICM_LAYOUT_FILE"); }
    if (layout_file == NULL) { layout_file = "sicm.layout";             }

    parse_layout_file(layout_file);

    if (!layout.is_valid) {
        ERR("Invalid layout.\n");
    }
}

void sicm_layout_fini(void) {
    tree_it(str, sicm_layout_node_t)  it;
    const char                       *key;

    while (tree_len(layout.nodes) > 0) {
        it  = tree_begin(layout.nodes);
        key = tree_it_key(it);
        tree_delete(layout.nodes, key);
        free(key);
    }

    tree_free(layout.nodes);
}

void * sicm_node_alloc(size_t size, const char *node_name) {
    /*
     * @incomplete
     */
    return NULL;
}

void * sicm_attr_alloc(size_t size, int attrs) {
    /*
     * @incomplete
     */
    return NULL;
}
