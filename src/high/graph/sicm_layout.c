#include "sicm_layout.h"

static sicm_layout_t the_layout;

static void parse_layout_file(const char *layout_file) {

}

void sicm_layout_init(const char *layout_file) {
    if (layout_file == NULL) { layout_file = getenv("SICM_LAYOUT_FILE"); }
    if (layout_file == NULL) { layout_file = ".sicm_layout";             }

    parse_layout_file(layout_file); 
}

void sicm_layout_fini(void) {
    tree_it(str, sicm_layout_node_t)  it;
    const char                       *key;

    while (tree_len(the_layout.nodes) > 0) {
        it  = tree_begin(the_layout.nodes);
        key = tree_it_key(it);
        tree_delete(key);
        free(key);
    }

    tree_free(the_layout.nodes);
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
