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

/* BEG Parsing functions */

typedef struct {
    const char *path;
    FILE *f;
    int  current_line;
} parse_info;

static int optional_int(parse_info *info, int *out) {
    return 0;
}

static int optional_word(parse_info *info, const char **out) {
    return 0;
}

static int optional_keyword(parse_info *info, const char* s) {
    return 0;
}

static void expect_int(parse_info *info, int *out) {
    int result;

    if (!optional_int(info, &result)) {
        ERR("invalid layout file '%s' -- expected int on line %d\n", info->path, info->current_line);
    }

    if (out)    { *out = result; }
}

static void expect_word(parse_info *info, int *out) {
    const char *result;

    if (!optional_word(info, &result)) {
        ERR("invalid layout file '%s' -- expected word on line %d\n", info->path, info->current_line);
    }

    if (out)    { *out = result; }
}

static void expect_keyword(parse_info *info, const char *s) {
    if (!optional_keyword(info, s)) {
        ERR("invalid layout file '%s' -- expected keyword '%s' on line %d\n", info->path, s, info->current_line);
    }
}

/* END Parsing functions */

static void parse_layout_file(const char *layout_file) {
    parse_info info;

    info.f            = fopen(layout_file, "r");
    info.path         = layout_file;
    info.current_line = 1;

    LOG("using layout file '%s'\n", info.path);

    if (info.f == NULL) {
        ERR("Could not open layout file '%s'.\n", info.path);
    }

    layout.nodes = tree_make(str, sicm_layout_node_t);

    expect_keyword(&info, "layout");
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
