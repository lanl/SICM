#include <errno.h>
#include <numa.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/mman.h>

#include "nano.h"
#include "sicm_low.h"
#include "sizes.h"

struct ThreadArgs {
    sicm_device_list *devs;
    sicm_device *src;
    sicm_device *dst;
    size_t allocations;
    size_t *sizes;
};

void *malloc_thread(void *ptr) {
    struct ThreadArgs *args = ptr;
    const size_t ptr_size = args->allocations * sizeof(void *);
    void **ptrs = numa_alloc_onnode(ptr_size, 0);
    memset(ptrs, 0, ptr_size);

    struct timespec start, end;
    clock_gettime(CLOCK_MONOTONIC, &start);

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

    /* bulk move */
    if (numa_move_pages(0, args->allocations, ptrs, nodes, statuses, 0) != 0) {
        const int err = errno;
        fprintf(stderr, "Move pages to src failed: %s %d\n", strerror(err), err);
        return NULL;
    }

    /* free */
    for(size_t i = 0; i < args->allocations; i++) {
        munmap(ptrs[i], args->sizes[i]);
        ptrs[i] = NULL;
    }

    numa_free(statuses, int_size);
    numa_free(nodes, int_size);

    clock_gettime(CLOCK_MONOTONIC, &end);
    double *elapsed = numa_alloc_onnode(sizeof(double), 0);
    *elapsed = nano(&start, &end);

    numa_free(ptrs, ptr_size);

    return elapsed;
}


void *mmap_thread(void *ptr) {
    struct ThreadArgs *args = ptr;
    const size_t ptr_size = args->allocations * sizeof(void *);
    void **ptrs = numa_alloc_onnode(ptr_size, 0);
    memset(ptrs, 0, ptr_size);

    struct timespec start, end;
    clock_gettime(CLOCK_MONOTONIC, &start);

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

    /* bulk move */
    if (numa_move_pages(0, args->allocations, ptrs, nodes, statuses, 0) != 0) {
        const int err = errno;
        fprintf(stderr, "Move pages to src failed: %s %d\n", strerror(err), err);
        return NULL;
    }

    /* free */
    for(size_t i = 0; i < args->allocations; i++) {
        munmap(ptrs[i], args->sizes[i]);
        ptrs[i] = NULL;
    }

    numa_free(statuses, int_size);
    numa_free(nodes, int_size);

    clock_gettime(CLOCK_MONOTONIC, &end);
    double *elapsed = numa_alloc_onnode(sizeof(double), 0);
    *elapsed = nano(&start, &end);

    numa_free(ptrs, ptr_size);

    return elapsed;
}

void *numa_thread(void *ptr) {
    struct ThreadArgs *args = ptr;
    const size_t ptr_size = args->allocations * sizeof(void *);
    void **ptrs = numa_alloc_onnode(ptr_size, 0);
    memset(ptrs, 0, ptr_size);

    struct timespec start, end;
    clock_gettime(CLOCK_MONOTONIC, &start);

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

    /* bulk move */
    if (numa_move_pages(0, args->allocations, ptrs, nodes, statuses, 0) != 0) {
        const int err = errno;
        fprintf(stderr, "Move pages to src failed: %s %d\n", strerror(err), err);
        return NULL;
    }

    /* free */
    for(size_t i = 0; i < args->allocations; i++) {
        munmap(ptrs[i], args->sizes[i]);
        ptrs[i] = NULL;
    }

    numa_free(statuses, int_size);
    numa_free(nodes, int_size);

    clock_gettime(CLOCK_MONOTONIC, &end);
    double *elapsed = numa_alloc_onnode(sizeof(double), 0);
    *elapsed = nano(&start, &end);

    numa_free(ptrs, ptr_size);

    return elapsed;
}

void *sicm_thread(void *ptr) {
    struct ThreadArgs *args = ptr;
    const size_t ptr_size = args->allocations * sizeof(void *);
    void **ptrs = numa_alloc_onnode(ptr_size, 0);
    memset(ptrs, 0, ptr_size);

    struct timespec start, end;
    clock_gettime(CLOCK_MONOTONIC, &start);

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

    /* bulk move */
    sicm_arena_set_device(arena, args->dst);

    /* frees all pointers in the arena*/
    sicm_arena_destroy(arena);

    clock_gettime(CLOCK_MONOTONIC, &end);
    double *elapsed = numa_alloc_onnode(sizeof(double), 0);
    *elapsed = nano(&start, &end);

    numa_free(ptrs, ptr_size);

    return elapsed;
}

void run(sicm_device_list *devs,
         const size_t thread_count,
         const size_t allocations,
         size_t *sizes,
         const char *name,
         void *(*func)(void *)) {
    /* move from all NUMA nodes to all NUMA nodes */
    for(int src_idx = 0; src_idx < devs->count; src_idx += 3) {
        for(int dst_idx = 0; dst_idx < devs->count; dst_idx += 3) {
            pthread_t *threads          = calloc(thread_count, sizeof(pthread_t));
            struct ThreadArgs *args     = calloc(thread_count, sizeof(struct ThreadArgs));

            struct timespec start, end;
            clock_gettime(CLOCK_MONOTONIC, &start);
            for(int i = 0; i < thread_count; i++) {
                args[i].devs = devs;
                args[i].src = devs->devices[src_idx];
                args[i].dst = devs->devices[dst_idx];
                args[i].allocations = allocations;
                args[i].sizes = sizes;
                if (pthread_create(&threads[i], NULL, func, &args[i]) != 0) {
                    fprintf(stderr, "Could not create thread %d of %s\n", i, name);
                    for(int j = 0; j < thread_count; j++) {
                        pthread_join(threads[j], NULL);
                        threads[j] = 0;
                    }
                    break;
                }
            }

            /* collect each thread's move time */
            double elapsed = 0;
            for(int i = 0; i < thread_count; i++) {
                void *thread_elapsed = NULL;
                pthread_join(threads[i], &thread_elapsed);
                if (thread_elapsed) {
                    elapsed += * (double *) thread_elapsed;
                }
                numa_free(thread_elapsed, sizeof(double));
            }
            clock_gettime(CLOCK_MONOTONIC, &end);

            free(args);
            free(threads);

            printf("%-10s %d -> %d %10.3fs %12.3fs\n",
                   name,
                   devs->devices[src_idx]->node,
                   devs->devices[dst_idx]->node,
                   nano(&start, &end) / 1e9, elapsed / 1e9);
        }
    }
}

int main(int argc, char *argv[]) {
    if (argc < 5) {
        fprintf(stderr, "Syntax: %s alloc_func threads size_count sizes\n", argv[0]);
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

    sicm_device_list devs = sicm_init();

    printf("Found %u NUMA nodes:\n", devs.count / 3);
    for(size_t i = 0; i < devs.count; i += 3) {
        printf("    %d %s\n", devs.devices[i]->node,
               sicm_device_tag_str(devs.devices[i]->tag));
    }

    printf("%10s        %12s   %12s\n", "", "RealTime", "ThreadTime");
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
        run(&devs, thread_count, count, sizes, alloc_func, thread);
    }

    sicm_fini();

    free(sizes);

    return 0;
}
