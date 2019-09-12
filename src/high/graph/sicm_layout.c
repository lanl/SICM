#include "sicm_layout.h"

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <ctype.h>

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
            if (c == '\n') {
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

static int optional_word(parse_info *info, const char **out) {
    char        c;
    char        word_buff[256];
    char       *buff_p;
    int         len;
    
    buff_p = word_buff;

    while ((c = *info->cursor) && !isspace(c)) {
        *(buff_p++)   = c;
        info->cursor += 1;
        len          += 1;

        if (len == 255) {
            ERR("word too long to parse on line %d\n", info->current_line);
        }
    }

    *buff_p = 0;

    if (out && len) {
        *out = malloc(len + 1);
        memcpy(*out, word_buff, len + 1);
    }

    if (len) {
        LOG("parsed word '%s'\n", word_buff);
        trim_whitespace_and_comments(info);
    }

    return len;
}

static int optional_keyword(parse_info *info, const char* s) {
    char        c;
    int         len;
    const char *s_p,
               *cursor_save;

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

    trim_whitespace_and_comments(info);

    return 1;
}

static long int optional_int(parse_info *info, long int *out) {
    long int i;
    char     buff[256];

    if (sscanf(info->cursor, "%ld", &i) == 0) {
        return 0;
    }

    sprintf(buff, "%ld", i);

    info->cursor += strlen(buff);

    if (out) {
        *out = i;
    }

    trim_whitespace_and_comments(info);

    return 0;
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

static void expect_int(parse_info *info, long int *out) {
    long int result;

    if (!optional_int(info, &result)) {
        ERR("invalid layout file '%s' -- expected int on line %d\n", info->path, info->current_line);
    }

    if (out)    { *out = result; }
}

/* END Parsing functions */

static void parse_layout_file(const char *layout_file) {
    parse_info info;
    const char *layout_name;

    info = parse_info_make(layout_file);

    LOG("using layout file '%s'\n", info.path);

    layout.nodes = tree_make(str, sicm_layout_node_t);

    trim_whitespace_and_comments(&info);
    expect_int(&info, NULL);
    expect_keyword(&info, "layout");

    expect_word(&info, &layout_name);

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
