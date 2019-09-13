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

static sicm_layout_t layout;

/* BEG Parsing functions */

typedef struct {
    const char *path;
    char *buff, *cursor;
    int  current_line;
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

static void parse_info_free(parse_info *info) {
    free(info->buff);
}

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
            ERR("word too long to parse on line %d\n", info->current_line);
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
        parse_error(info, "expected keyword '%s'\n", s);
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

static sicm_layout_node_ptr get_node(parse_info *info, const char *name, int line) {
    tree_it(str, sicm_layout_node_ptr) it;

    it = tree_lookup(layout.nodes, name);

    if (!tree_it_good(it)) {
        parse_error_l(info, line, "node '%s' not defined yet -- can't create an edge with it\n", name);
    }

    return tree_it_val(it);
}

static sicm_layout_node_ptr get_or_create_node(const char *name) {
    tree_it(str, sicm_layout_node_ptr) it;
    sicm_layout_node_ptr               node;

    it = tree_lookup(layout.nodes, name);

    if (tree_it_good(it)) {
        node = tree_it_val(it);
    } else {
        node = malloc(sizeof(*node));
        memset(node, 0, sizeof(*node));
        node->name  = strdup(name);
        node->edges = tree_make_c(str, sicm_layout_edge_t, strcmp);

        tree_insert(layout.nodes, node->name, node);
    }
    
    return node;
}

static int parse_attr(parse_info *info, sicm_layout_node_ptr current_node, const char *attr) {
    int line;

    if ((line = optional_keyword(info, attr))) {
        if (!current_node) {
            parse_error_l(info, line, "can't set '%s' for unspecified node\n", attr);
        }
    }

    return line;
}

static int parse_int_value(parse_info *info, sicm_layout_node_ptr current_node, const char *kwd, long int *integer) {
    int line;

    if ((line = optional_keyword(info, kwd))) {
        if (!current_node) {
            parse_error_l(info, line, "can't set '%s' for unspecified node\n", kwd);
        }
    
        expect_int(info, integer);
    }

    return line;
}

static int parse_kind(parse_info *info, sicm_layout_node_ptr current_node, long int *kind) {
    int line;

    if ((line = optional_keyword(info, "kind"))) {
        if (!current_node) {
            parse_error_l(info, line, "can't set 'kind' for unspecified node\n");
        }
        if (optional_keyword(info, "mem")) {
            *kind = NODE_MEM;
            current_node->kind = NODE_MEM;
        } else if (optional_keyword(info, "compute")) {
            *kind = NODE_COMPUTE;
        } else {
            parse_error(info, "expected either 'mem' or 'compute'\n");
        }

        return 1;
    }

    return 0;
}

static void parse_layout_file(const char *layout_file) {
    parse_info           info;
    sicm_layout_node_ptr current_node,
                         src_node,
                         dst_node;
    char                 buff[WORD_MAX];
    long int             integer;
    int                  line;

    info         = parse_info_make(layout_file);
    current_node = NULL;

    layout.name  = malloc(WORD_MAX);
    layout.nodes = tree_make_c(str, sicm_layout_node_ptr, strcmp);

    trim_whitespace_and_comments(&info);
  
    expect_keyword(&info, "layout");
    expect_word(&info, layout.name);

    while (*info.cursor) {
        /* 
         * Create a new node or select an existing one.
         */
        if (optional_keyword(&info, "node")) {
            expect_word(&info, buff);
            current_node = get_or_create_node(buff);

        /*
         * Set properties of the selected node.
         */
        } else if (parse_kind(&info, current_node, &integer)) {
            current_node->kind = integer; 

        } else if (parse_int_value(&info, current_node, "numa", &integer)) {
            current_node->numa_node_id = integer;

        } else if (parse_int_value(&info, current_node, "capacity", &integer)) {
            current_node->capacity = integer;

        } else if (parse_attr(&info, current_node, "near_nic")) {
            current_node->attrs |= NODE_NEAR_NIC;

        } else if (parse_attr(&info, current_node, "low_lat")) {
            current_node->attrs |= NODE_LOW_LAT;

        } else if (parse_attr(&info, current_node, "hbm")) {
            current_node->attrs |= NODE_HBM;

        } else if (parse_attr(&info, current_node, "nvm")) {
            current_node->attrs |= NODE_NVM;

        } else if (parse_attr(&info, current_node, "gpu")) {
            current_node->attrs |= NODE_ON_GPU;

        /*
         * Parse an edge.
         */
        } else if (optional_keyword(&info, "edge")) {
            line     = expect_word(&info, buff);
            src_node = get_node(&info, buff, line);
        } else {
            if (optional_word(&info, &buff)) {
                parse_error(&info, "did not expect '%s' here\n", buff);
            } else {
                parse_error(&info, "did not expect the end of the file\n");
            }
        }
    }

    parse_info_free(&info);

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
    tree_it(str, sicm_layout_node_ptr)  it;
    const char                         *key;
    sicm_layout_node_ptr                val;

    free(layout.name);

    while (tree_len(layout.nodes) > 0) {
        it  = tree_begin(layout.nodes);
        key = tree_it_key(it);
        val = tree_it_val(it);
        tree_delete(layout.nodes, key);
        free(key);
        free(val);
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
