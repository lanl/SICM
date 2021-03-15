#include <stdio.h>
#include <stdlib.h>
#include <time.h>

#include "sizes.h"

int main(int argc, char *argv[]) {
    srand(time(NULL));

    if (argc < 3) {
        fprintf(stderr, "Syntax: %s max_size allocation_count\n", argv[0]);
        return 1;
    }

    size_t max_size = 0;
    if ((sscanf(argv[1], "%zu", &max_size) != 1) || !max_size) {
        fprintf(stderr, "Bad max size: %s\n", argv[1]);
        return 1;
    }

    size_t count = 0;
    if (sscanf(argv[2], "%zu", &count) != 1) {
        fprintf(stderr, "Bad allocation count: %s\n", argv[2]);
        return 1;
    }

    size_t *sizes = select_sizes(max_size, count);
    print_sizes(sizes, count);
    free(sizes);

    return 0;
}
