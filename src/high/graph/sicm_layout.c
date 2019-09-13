#include "sicm_layout.h"

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <ctype.h>
#include <stdarg.h>


#define WORD_MAX (256)


#define ERR(...) do {                                     \
    fprintf(stderr, "[sicm-layout] ERROR: " __VA_ARGS__); \
    exit(1);                                              \
} while (0)

#define LOG(...) do {                                     \
    fprintf(stderr, "[sicm-layout]:       " __VA_ARGS__); \
} while (0)

static sl_t layout;

/* BEG Parsing functions */

typedef struct {
    const char *path;
    char       *buff,
               *cursor;
    int         current_line;
} parse_info;

static parse_info parse_info_make(const char *path) {
    parse_info  info;
    FILE       *f;
    size_t      buff_size,
                n_read;

    f = fopen(path, "r");
    if (f == NULL) {
        ERR("Could not open layout file '%s'.\n", path);
    }

    info.path         = path;
    info.current_line = 1;

    /*
     * Get the size of the file and allocate the buffer.
     */
    fseek(f, 0, SEEK_END);
    buff_size = ftell(f);
    rewind(f);

    buff_size  += 1;
    info.cursor = info.buff = malloc(buff_size);

    n_read      = fread(info.buff, 1, buff_size - 1, f);

    if (n_read != (buff_size - 1)) {
        ERR("encountered a problem attempting to read the contents of '%s' "
            "-- read %llu of %llu bytes\n", path, n_read, buff_size);
    }

    /*
     * NULL term.
     */
    info.buff[buff_size - 1] = 0;

    fclose(f);

    return info;
}

static void parse_info_free(parse_info *info) { free(info->buff); }

static void trim_comment(parse_info *info) {
    char c;

    c = *info->cursor;

    if (c != '#')    { return; }

    while ((c = *(++info->cursor))) {
        if (c == '\n')    {
            info->current_line += 1;
            break;
        }
    }
}

static void trim_whitespace_and_comments(parse_info *info) {
    char c;

    while ((c = *info->cursor)) {
        if (isspace(c)) {
            if (c == '\n' && *(info->cursor + 1)) {
                info->current_line += 1;
            }
        } else if (c == '#') {
            trim_comment(info);
        } else {
            break;
        }

        info->cursor += 1;
    }
}

static void vparse_error_l(parse_info *info, int line, const char *fmt, va_list args) {
    fprintf(stderr, "[sicm-layout]: PARSE ERROR in '%s' :: line %d\n"
                    "               ", info->path, line);
    vfprintf(stderr, fmt, args);
    exit(1);
}

static void parse_error_l(parse_info *info, int line, const char *fmt, ...) {
    va_list args;

    va_start(args, fmt);
    vparse_error_l(info, line, fmt, args);
    va_end(args);
}

static void parse_error(parse_info *info, const char *fmt, ...) {
    va_list args;

    va_start(args, fmt);
    vparse_error_l(info, info->current_line, fmt, args);
    va_end(args);
}

static int optional_word(parse_info *info, const char *out) {
    char  c;
    char  word_buff[WORD_MAX];
    char *buff_p;
    int   len;
    int   line;

    len    = 0;
    buff_p = word_buff;

    while ((c = *info->cursor) && !isspace(c)) {
        *(buff_p++)   = c;
        info->cursor += 1;
        len          += 1;

        if (len == WORD_MAX - 1) {
            *buff_p = 0;
            parse_error(info, "word '%s' is too long -- max word length is %d\n", word_buff, WORD_MAX - 1);
        }
    }

    *buff_p = 0;

    if (out && len) {
        memcpy(out, word_buff, len + 1);
    }

    if (!len) {
        return 0;
    }

    line = info->current_line;

    trim_whitespace_and_comments(info);

    return line;
}

static int optional_keyword(parse_info *info, const char* s) {
    char        c;
    int         len;
    const char *s_p,
               *cursor_save;
    int         line;

    len         = 0;
    s_p         = s;
    cursor_save = info->cursor;

    while (*info->cursor && *s_p && (*s_p == *info->cursor)) {
        len += 1;
        s_p += 1;
        info->cursor += 1;
    }

    if (*info->cursor &&
        !isspace(*info->cursor) && *info->cursor != '#') {

        info->cursor = cursor_save;
        return 0;
    }

    if (len != strlen(s)) {
        info->cursor = cursor_save;
        return 0;
    }

    line = info->current_line;

    trim_whitespace_and_comments(info);

    return line;
}

static long int optional_int(parse_info *info, long int *out) {
    long int i;
    char     buff[WORD_MAX];
    int      line;

    if (!*info->cursor || sscanf(info->cursor, "%ld", &i) == 0) {
        return 0;
    }

    sprintf(buff, "%ld", i);

    info->cursor += strlen(buff);

    if (out)    { *out = i; }

    line = info->current_line;

    trim_whitespace_and_comments(info);

    return line;
}

static int expect_word(parse_info *info, const char *out) {
    int line;

    if (!(line = optional_word(info, out))) {
        parse_error(info, "expected a word\n");
    }

    return line;
}

static int expect_keyword(parse_info *info, const char *s) {
    int line;

    if (!(line = optional_keyword(info, s))) {
        parse_error(info, "expected '%s'\n", s);
    }

    return line;
}

static int expect_int(parse_info *info, long int *out) {
    int line;

    if (!(line = optional_int(info, out))) {
        parse_error(info, "expected an integer\n");
    }

    return line;
}

/* END Parsing functions */

/* BEG Private functions */

static sl_node_ptr get_node(parse_info *info, const char *name, int line) {
    tree_it(sl_str, sl_node_ptr) it;

    it = tree_lookup(layout.nodes, name);

    if (!tree_it_good(it)) {
        parse_error_l(info, line, "node '%s' not defined yet -- can't create an edge with it\n", name);
    }

    return tree_it_val(it);
}

static sl_node_ptr get_or_create_node(const char *name) {
    tree_it(sl_str, sl_node_ptr) it;
    sl_node_ptr                  node;

    it = tree_lookup(layout.nodes, name);

    if (tree_it_good(it)) {
        node = tree_it_val(it);
    } else {
        node = malloc(sizeof(*node));
        memset(node, 0, sizeof(*node));

        node->name         = strdup(name);
        node->line         = SL_NODE_LINE_UNKNOWN;
        node->numa_node_id = SL_NODE_NUMA_UNKNOWN;
        node->kind         = SL_NODE_UNKNOWN;
        node->attrs        = 0;
        node->capacity     = SL_NODE_CAP_UNKNOWN;
        node->edges        = tree_make_c(sl_str, sl_edge_ptr, strcmp);

        tree_insert(layout.nodes, node->name, node);
    }

    return node;
}

static int parse_node_attr(parse_info *info, sl_node_ptr current_node, const char *attr) {
    int line;

    if ((line = optional_keyword(info, attr))) {
        if (!current_node) {
            parse_error_l(info, line, "can't set '%s' for unspecified node\n", attr);
        }
    }

    return line;
}

static int parse_node_int_value(parse_info *info, sl_node_ptr current_node, const char *kwd, long int *integer) {
    int line;

    if ((line = optional_keyword(info, kwd))) {
        if (!current_node) {
            parse_error_l(info, line, "can't set '%s' for unspecified node\n", kwd);
        }

        expect_int(info, integer);
    }

    return line;
}

static int parse_node_kind(parse_info *info, sl_node_ptr current_node, long int *kind) {
    int line;

    if ((line = optional_keyword(info, "kind"))) {
        if (!current_node) {
            parse_error_l(info, line, "can't set 'kind' for unspecified node\n");
        }
        if (optional_keyword(info, "mem")) {
            *kind = SL_NODE_MEM;
            current_node->kind = SL_NODE_MEM;
        } else if (optional_keyword(info, "compute")) {
            *kind = SL_NODE_COMPUTE;
        } else {
            parse_error(info, "expected either 'mem' or 'compute'\n");
        }

        return 1;
    }

    return 0;
}

static sl_edge_ptr get_edge(sl_node_ptr src_node, sl_node_ptr dst_node) {
    tree_it(sl_str, sl_edge_ptr) edge_it;
    sl_edge_ptr                  new_edge;

    edge_it = tree_lookup(src_node->edges, dst_node->name);

    if (tree_it_good(edge_it)) {
        return tree_it_val(edge_it);
    }

    new_edge      = malloc(sizeof(*new_edge));
    new_edge->bw  = SL_EDGE_BW_UNKNOWN;
    new_edge->lat = SL_EDGE_LAT_UNKNOWN;

    tree_insert(src_node->edges, strdup(dst_node->name), new_edge);

    return new_edge;
}

static void parse_layout_file(const char *layout_file) {
    parse_info  info;
    sl_node_ptr current_node,
                src_node,
                dst_node;
    char        buff[WORD_MAX];
    long int    integer;
    int         line;
    sl_edge_ptr edge1,
                edge2;

    info         = parse_info_make(layout_file);
    current_node = NULL;

    layout.name  = malloc(WORD_MAX);
    layout.path  = strdup(layout_file);
    layout.nodes = tree_make_c(sl_str, sl_node_ptr, strcmp);

    trim_whitespace_and_comments(&info);

    expect_keyword(&info, "layout");
    expect_word(&info, layout.name);

    while (*info.cursor) {
        /*
         * Create a new node or select an existing one.
         */
        if ((line = optional_keyword(&info, "node"))) {
            expect_word(&info, buff);
            current_node = get_or_create_node(buff);
            if (current_node->line == SL_NODE_LINE_UNKNOWN) {
                current_node->line = line;
            }
        /*
         * Set properties of the selected node.
         */
        } else if (parse_node_kind(&info, current_node, &integer)) {
            current_node->kind = integer;

        } else if (parse_node_int_value(&info, current_node, "numa", &integer)) {
            current_node->numa_node_id = integer;

        } else if (parse_node_int_value(&info, current_node, "capacity", &integer)) {
            current_node->capacity = integer;

        } else if (parse_node_attr(&info, current_node, "near_nic")) {
            current_node->attrs |= SL_NODE_NEAR_NIC;

        } else if (parse_node_attr(&info, current_node, "hbm")) {
            current_node->attrs |= SL_NODE_HBM;

        } else if (parse_node_attr(&info, current_node, "nvm")) {
            current_node->attrs |= SL_NODE_NVM;

        } else if (parse_node_attr(&info, current_node, "gpu")) {
            current_node->attrs |= SL_NODE_GPU;

        /*
         * Parse an edge.
         */
        } else if (optional_keyword(&info, "edge")) {
            line     = expect_word(&info, buff);
            src_node = get_node(&info, buff, line);
            expect_keyword(&info, "->");
            line     = expect_word(&info, buff);
            dst_node = get_node(&info, buff, line);

            edge1 = get_edge(src_node, dst_node);
            edge2 = get_edge(dst_node, src_node);

            while (1) {
                if (optional_keyword(&info, "bandwidth")) {
                    expect_int(&info, &integer);
                    edge1->bw = edge2->bw = integer;
                } else if (optional_keyword(&info, "latency")) {
                    expect_int(&info, &integer);
                    edge1->lat = edge2->lat = integer;
                } else {
                    break;
                }
            }

        /*
         * Invalid input.
         */
        } else {
            if (optional_word(&info, &buff)) {
                parse_error(&info, "did not expect '%s' here\n", buff);
            } else {
                parse_error(&info, "did not expect the end of the file\n");
            }
        }
    }

    parse_info_free(&info);
}

static void layout_node_error(sl_node_ptr node, const char *fmt, ...) {
    va_list args;

    va_start(args, fmt);
    fprintf(stderr, "[sicm-layout]: SL ERROR in '%s' :: for node '%s' defined on line %d\n"
                    "               ", layout.path, node->name, node->line);
    vfprintf(stderr, fmt, args);
    exit(1);
    va_end(args);
}

static void verify_node(sl_node_ptr node) {
    if (node->kind == SL_NODE_COMPUTE) {
        if (node->capacity != SL_NODE_CAP_UNKNOWN) {
            layout_node_error(node, "has conflicting attributes 'kind compute' and 'capacity'\n");
        }
        if (node->attrs & SL_NODE_HBM) {
            layout_node_error(node, "has conflicting attributes 'kind compute' and 'hbm'\n");
        }
        if (node->attrs & SL_NODE_NVM) {
            layout_node_error(node, "has conflicting attributes 'kind compute' and 'nvm'\n");
        }
    } else if (node->kind == SL_NODE_MEM) {
        /*
         * Not really anything to check here..
         */
    } else {
        layout_node_error(node, "missing required attribute 'kind'\n");
    }

    if (node->numa_node_id == SL_NODE_NUMA_UNKNOWN) {
        layout_node_error(node, "missing required attribute 'numa'\n");
    }
}

static void verify_layout() {
    tree_it(sl_str, sl_node_ptr) node_it;

    tree_traverse(layout.nodes, node_it) {
        verify_node(tree_it_val(node_it));
    }
}

/* END Private functions */

/* BEG Public functions */

void sl_init(const char *layout_file) {
    if (layout_file == NULL) { layout_file = getenv("SICM_SL_FILE"); }
    if (layout_file == NULL) { layout_file = "sicm.layout";             }

    parse_layout_file(layout_file);
    verify_layout();

    layout.is_valid = 1;
}

void sl_fini(void) {
    tree_it(sl_str,
            sl_node_ptr)  node_it;
    tree_it(sl_str,
            sl_edge_ptr)  edge_it;
    const char                    *node_key,
                                  *edge_key;
    sl_node_ptr           node_val;
    sl_edge_ptr           edge_val;

    free(layout.name);
    free(layout.path);

    /*
     * Free each node.
     */
    while (tree_len(layout.nodes) > 0) {
        node_it  = tree_begin(layout.nodes);
        node_key = tree_it_key(node_it);
        node_val = tree_it_val(node_it);

        tree_delete(layout.nodes, node_key);

        free(node_key);

        /*
         * Free each edge.
         */
        while (tree_len(node_val->edges) > 0) {
            edge_it  = tree_begin(node_val->edges);
            edge_key = tree_it_key(edge_it);
            edge_val = tree_it_val(edge_it);

            tree_delete(node_val->edges, edge_key);

            free(edge_key);
            free(edge_val);
        }

        free(node_val);
    }

    tree_free(layout.nodes);
}

int sl_num_nodes() {
    if (!layout.is_valid) {
        ERR("Invalid layout. Perhaps sl_init() wasn't called?\n");
    }

    return tree_len(layout.nodes);
}

sl_node_handle * sl_nodes() {
    tree_it(sl_str, sl_node_ptr) node_it;
    int                                            n_nodes,
                                                   i;
    /*
     * Get number of nodes and check for valid layout.
     */
    n_nodes = sl_num_nodes();

    if (layout.flat_nodes == NULL) {
        layout.flat_nodes = malloc(n_nodes * sizeof(*layout.flat_nodes));

        i = 0;
        tree_traverse(layout.nodes, node_it) {
            layout.flat_nodes[i++] = tree_it_key(node_it);
        }
    }

    return layout.flat_nodes;
}

static sl_node_ptr find_existing_node(sl_node_handle handle) {
    tree_it(sl_str, sl_node_ptr) node_it;

    if (!layout.is_valid) {
        ERR("Invalid layout. Perhaps sl_init() wasn't called?\n");
    }

    node_it = tree_lookup(layout.nodes, handle);

    if (!tree_it_good(node_it)) {
        ERR("Node '%s' not found in layout. Only use node handles provided by sl_nodes().\n", handle);
    }

    return tree_it_val(node_it);
}

const char * sl_node_name(sl_node_handle handle) { return handle; }

int sl_node_kind(sl_node_handle handle) {
    sl_node_ptr node;

    node = find_existing_node(handle);

    return node->kind;
}

long int sl_node_numa(sl_node_handle handle) {
    sl_node_ptr node;

    node = find_existing_node(handle);

    return node->numa_node_id;
}

long int sl_node_capacity(sl_node_handle handle) {
    sl_node_ptr node;

    node = find_existing_node(handle);

    return node->capacity;
}

int sl_node_is_near_nic(sl_node_handle handle) {
    sl_node_ptr node;

    node = find_existing_node(handle);

    return node->attrs & SL_NODE_NEAR_NIC;
}

int sl_node_is_hbm(sl_node_handle handle) {
    sl_node_ptr node;

    node = find_existing_node(handle);

    return node->attrs & SL_NODE_HBM;
}

int sl_node_is_nvm(sl_node_handle handle) {
    sl_node_ptr node;

    node = find_existing_node(handle);

    return node->attrs & SL_NODE_NVM;
}

int sl_node_is_gpu(sl_node_handle handle) {
    sl_node_ptr node;

    node = find_existing_node(handle);

    return node->attrs & SL_NODE_GPU;
}

sl_edge_handle sl_edge(sl_node_handle src, sl_node_handle dst) {
    sl_node_ptr                  src_node;
    tree_it(sl_str, sl_edge_ptr) edge_it;

    src_node = find_existing_node(src);
    edge_it  = tree_lookup(src_node->edges, dst);

    if (!tree_it_good(edge_it)) {
        return SL_NO_EDGE;
    }

    return tree_it_val(edge_it);
}

long int sl_edge_bandwidth(sl_edge_handle handle) { return handle->bw;  }
long int sl_edge_latency(sl_edge_handle handle)   { return handle->lat; }

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

/* END Public functions */
