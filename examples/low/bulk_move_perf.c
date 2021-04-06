#include <errno.h>
#include <numa.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/mman.h>
#include <time.h>

#include "run_move.h"
#include "sicm_low.h"
#include "sizes.h"

void *malloc_thread(void *ptr) {
    struct ThreadArgs *args = ptr;

    if (pin_thread(args->id) != 0) {
        fprintf(stderr, "Could not pin thread to cpu %zu\n", args->id);
        return NULL;
    }

    const size_t ptr_size = args->allocations * sizeof(void *);
    void **ptrs = numa_alloc_onnode(ptr_size, 0);
    memset(ptrs, 0, ptr_size);

    const size_t int_size = args->allocations * sizeof(int);
    int *nodes = numa_alloc_onnode(int_size, 0);
    int *statuses = numa_alloc_onnode(int_size, 0);

    /* allocate */
    for(size_t i = 0; i < args->allocations; i++) {
        nodes[i] = args->src->node;

        ptrs[i] = malloc(args->sizes[i]);

        if (!ptrs[i]) {
            fprintf(stderr, "Could not allocate ptrs[%zu]\n", i);
        }

        memset(ptrs[i], 0, args->sizes[i]);
    }

    if (numa_move_pages(0, args->allocations, ptrs, nodes, statuses, 0) != 0) {
        const int err = errno;
        fprintf(stderr, "Move pages to src failed: %s %d\n", strerror(err), err);
        return NULL;
    }

    for(size_t i = 0; i < args->allocations; i++) {
        nodes[i] = args->dst->node;
    }

    clock_gettime(CLOCK_MONOTONIC, &args->start);

    /* bulk move */
    if (numa_move_pages(0, args->allocations, ptrs, nodes, statuses, 0) != 0) {
        const int err = errno;
        fprintf(stderr, "Move pages to src failed: %s %d\n", strerror(err), err);
        return NULL;
    }

    clock_gettime(CLOCK_MONOTONIC, &args->end);

    /* free */
    for(size_t i = 0; i < args->allocations; i++) {
        free(ptrs[i]);
        ptrs[i] = NULL;
    }

    numa_free(statuses, int_size);
    numa_free(nodes, int_size);
    numa_free(ptrs, ptr_size);

    return NULL;
}

void *mmap_thread(void *ptr) {
    struct ThreadArgs *args = ptr;

    if (pin_thread(args->id) != 0) {
        fprintf(stderr, "Could not pin thread to cpu %zu\n", args->id);
        return NULL;
    }

    const size_t ptr_size = args->allocations * sizeof(void *);
    void **ptrs = numa_alloc_onnode(ptr_size, 0);
    memset(ptrs, 0, ptr_size);

    const size_t int_size = args->allocations * sizeof(int);
    int *nodes = numa_alloc_onnode(int_size, 0);
    int *statuses = numa_alloc_onnode(int_size, 0);

    /* allocate */
    for(size_t i = 0; i < args->allocations; i++) {
        nodes[i] = args->src->node;

        ptrs[i] = mmap(NULL, args->sizes[i], PROT_READ | PROT_WRITE,
                       MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);

        if (!ptrs[i]) {
            fprintf(stderr, "Could not allocate ptrs[%zu]\n", i);
        }

        memset(ptrs[i], 0, args->sizes[i]);
    }

    if (numa_move_pages(0, args->allocations, ptrs, nodes, statuses, 0) != 0) {
        const int err = errno;
        fprintf(stderr, "Move pages to src failed: %s %d\n", strerror(err), err);
        return NULL;
    }

    for(size_t i = 0; i < args->allocations; i++) {
        nodes[i] = args->dst->node;
    }

    clock_gettime(CLOCK_MONOTONIC, &args->start);

    /* bulk move */
    if (numa_move_pages(0, args->allocations, ptrs, nodes, statuses, 0) != 0) {
        const int err = errno;
        fprintf(stderr, "Move pages to src failed: %s %d\n", strerror(err), err);
        return NULL;
    }

    clock_gettime(CLOCK_MONOTONIC, &args->end);

    /* free */
    for(size_t i = 0; i < args->allocations; i++) {
        munmap(ptrs[i], args->sizes[i]);
        ptrs[i] = NULL;
    }

    numa_free(statuses, int_size);
    numa_free(nodes, int_size);
    numa_free(ptrs, ptr_size);

    return NULL;
}

void *numa_thread(void *ptr) {
    struct ThreadArgs *args = ptr;

    if (pin_thread(args->id) != 0) {
        fprintf(stderr, "Could not pin thread to cpu %zu\n", args->id);
        return NULL;
    }

    const size_t ptr_size = args->allocations * sizeof(void *);
    void **ptrs = numa_alloc_onnode(ptr_size, 0);
    memset(ptrs, 0, ptr_size);

    const size_t int_size = args->allocations * sizeof(int);
    int *nodes = numa_alloc_onnode(int_size, 0);
    int *statuses = numa_alloc_onnode(int_size, 0);

    /* allocate */
    for(size_t i = 0; i < args->allocations; i++) {
        nodes[i] = args->src->node;

        ptrs[i] = numa_alloc_onnode(args->sizes[i], nodes[i]);

        if (!ptrs[i]) {
            fprintf(stderr, "Could not allocate ptrs[%zu]\n", i);
        }

        memset(ptrs[i], 0, args->sizes[i]);
    }

    for(size_t i = 0; i < args->allocations; i++) {
        nodes[i] = args->dst->node;
    }

    if (numa_move_pages(0, args->allocations, ptrs, nodes, statuses, 0) != 0) {
        const int err = errno;
        fprintf(stderr, "Move pages to src failed: %s %d\n", strerror(err), err);
        return NULL;
    }

    clock_gettime(CLOCK_MONOTONIC, &args->start);

    /* bulk move */
    if (numa_move_pages(0, args->allocations, ptrs, nodes, statuses, 0) != 0) {
        const int err = errno;
        fprintf(stderr, "Move pages to src failed: %s %d\n", strerror(err), err);
        return NULL;
    }

    clock_gettime(CLOCK_MONOTONIC, &args->end);

    /* free */
    for(size_t i = 0; i < args->allocations; i++) {
        numa_free(ptrs[i], args->sizes[i]);
        ptrs[i] = NULL;
    }

    numa_free(statuses, int_size);
    numa_free(nodes, int_size);
    numa_free(ptrs, ptr_size);

    return NULL;
}

void *sicm_thread(void *ptr) {
    struct ThreadArgs *args = ptr;

    if (pin_thread(args->id) != 0) {
        fprintf(stderr, "Could not pin thread to cpu %zu\n", args->id);
        return NULL;
    }

    const size_t ptr_size = args->allocations * sizeof(void *);
    void **ptrs = numa_alloc_onnode(ptr_size, 0);
    memset(ptrs, 0, ptr_size);

    sicm_device_list srcs;
    srcs.count = 1;
    srcs.devices = &args->src;

    sicm_arena arena = sicm_arena_create(0, 0, &srcs);

    /* allocate */
    for(size_t i = 0; i < args->allocations; i++) {
        ptrs[i] = sicm_arena_alloc(arena, args->sizes[i]);

        if (!ptrs[i]) {
            fprintf(stderr, "Could not allocate ptrs[%zu]\n", i);
        }

        memset(ptrs[i], 0, args->sizes[i]);
    }

    clock_gettime(CLOCK_MONOTONIC, &args->start);

    /* bulk move */
    sicm_arena_set_device(arena, args->dst);

    clock_gettime(CLOCK_MONOTONIC, &args->end);

    /* frees all pointers in the arena */
    sicm_arena_destroy(arena);

    numa_free(ptrs, ptr_size);

    return NULL;
}

int main(int argc, char *argv[]) {
    if (argc < 5) {
        fprintf(stderr, "Syntax: %s alloc_func threads size_count sizes [reps]\n", argv[0]);
        return 1;
    }

    char *alloc_func = argv[1];

    size_t thread_count = 0;
    if (sscanf(argv[2], "%zu", &thread_count) != 1) {
        fprintf(stderr, "Bad thread count: %s\n", argv[2]);
        return 1;
    }

    size_t count = 0;
    if (sscanf(argv[3], "%zu", &count) != 1) {
        fprintf(stderr, "Bad allocation count: %s\n", argv[3]);
        return 1;
    }

    size_t *sizes = read_sizes(argv[4], count);
    if (!sizes) {
        fprintf(stderr, "Bad size file: %s\n", argv[4]);
        return 1;
    }

    size_t reps = 1;
    if (argc > 5) {
        if (sscanf(argv[5], "%zu", &reps) != 1) {
            fprintf(stderr, "Bad repetition count: %s\n", argv[5]);
            return 1;
        }
    }

    printf("%s x %zu Runs\n", alloc_func, reps);

    size_t thread_size = 0;
    for(size_t i = 0; i < count; i++) {
        thread_size += sizes[i];
    }
    printf("Total allocation size per run: %zu bytes / thread x %zu threads = %zu bytes \n", thread_size, thread_count, thread_size * thread_count);

    sicm_device_list devs = sicm_init();

    printf("%d", devs.count / 3);
    for(size_t i = 0; i < devs.count; i += 3) {
        printf(" %d", devs.devices[i]->node);
    }
    printf("\n");

    void *(*thread)(void *) = NULL;
    const size_t len = strlen(alloc_func);
    if ((len == 6) && (strncmp(alloc_func, "malloc", len) == 0)) {
        thread = malloc_thread;
    }
    else if ((len == 4) && (strncmp(alloc_func, "mmap", len) == 0)) {
        thread = mmap_thread;
    }
    else if ((len == 4) && (strncmp(alloc_func, "numa", len) == 0)) {
        thread = numa_thread;
    }
    else if ((len == 4) && (strncmp(alloc_func, "sicm", len) == 0)) {
        thread = sicm_thread;
    }
    else {
        fprintf(stderr, "Bad allocator function: %s\n", alloc_func);
    }

    if (thread) {
        run(&devs, thread_count, count, sizes, reps, alloc_func, thread);
    }

    sicm_fini();

    free(sizes);

    return 0;
}
