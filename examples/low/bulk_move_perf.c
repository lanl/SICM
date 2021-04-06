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

int numa_move_pages_wrapper(void **ptrs, size_t count, int *dsts, int *statuses,
                            sicm_arena arena, sicm_device *dst) {
    return numa_move_pages(0, count, ptrs, dsts, statuses, 0);
}

void *thread_common(struct ThreadArgs *args,
                    sicm_arena (*CREATE_ARENA)(sicm_device *, sicm_device_list *),
                    void *(*ALLOC)(const size_t, int, sicm_arena),
                    int (*ALLOC_MOVE)(void **, size_t, int *, int *, sicm_arena, sicm_device *),
                    int (*MOVE)(void **, size_t, int *, int *, sicm_arena, sicm_device *),
                    void (*FREE) (void *, const size_t),
                    void (*DESTROY_ARENA)(sicm_arena)) {
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

    /* only used by SICM */
    sicm_device_list srcs;
    sicm_arena arena;
    if (CREATE_ARENA) {
        arena = CREATE_ARENA(args->src, &srcs);
    }

    /* allocate */
    for(size_t i = 0; i < args->allocations; i++) {
        nodes[i] = args->src->node;

        ptrs[i] = ALLOC(args->sizes[i], nodes[i], arena);
        if (!ptrs[i]) {
            fprintf(stderr, "Could not allocate ptrs[%zu]\n", i);
        }

        memset(ptrs[i], 0, args->sizes[i]);
    }

    /* move ptrs to source NUMA node */
    if (ALLOC_MOVE) {
        if (numa_move_pages_wrapper(ptrs, args->allocations, nodes, statuses, NULL, NULL) != 0) {
            const int err = errno;
            fprintf(stderr, "Move pages to src failed: %s %d\n", strerror(err), err);
            return NULL;
        }
    }

    /* change array to send to dst NUMA node */
    for(size_t i = 0; i < args->allocations; i++) {
        nodes[i] = args->dst->node;
    }

    /* bulk move */
    clock_gettime(CLOCK_MONOTONIC, &args->start);
    const int rc = MOVE(ptrs, args->allocations, nodes, statuses, arena, args->dst);
    clock_gettime(CLOCK_MONOTONIC, &args->end);

    if (rc != 0) {
        const int err = errno;
        fprintf(stderr, "Move pages to src failed: %s %d\n", strerror(err), err);
    }

    /* free */
    if (FREE) {
        for(size_t i = 0; i < args->allocations; i++) {
            FREE(ptrs[i], args->sizes[i]);
            ptrs[i] = NULL;
        }
    }

    /* only used by SICM */
    if (DESTROY_ARENA) {
        DESTROY_ARENA(arena);
    }

    numa_free(statuses, int_size);
    numa_free(nodes, int_size);
    numa_free(ptrs, ptr_size);

    return NULL;
}

void *alloc_malloc(const size_t size, int src, sicm_arena arena) {
    return malloc(size);
}

void free_malloc(void *ptr, const size_t size) {
    free(ptr);
}

void *malloc_thread(void *ptr) {
    return thread_common(ptr,
                         NULL,
                         alloc_malloc, numa_move_pages_wrapper,
                         numa_move_pages_wrapper,
                         free_malloc,
                         NULL);
}

void *alloc_mmap(const size_t size, int src, sicm_arena arena) {
    return mmap(NULL, size, PROT_READ | PROT_WRITE,
                MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
}

void free_mmap(void *ptr, const size_t size) {
    munmap(ptr, size);
}

void *mmap_thread(void *ptr) {
    return thread_common(ptr,
                         NULL,
                         alloc_mmap, numa_move_pages_wrapper,
                         numa_move_pages_wrapper,
                         free_mmap,
                         NULL);
}

void *alloc_numa(const size_t size, int src, sicm_arena arena) {
    return numa_alloc_onnode(size, src);
}

void free_numa(void *ptr, const size_t size) {
    numa_free(ptr, size);
}

void *numa_thread(void *ptr) {
    return thread_common(ptr,
                         NULL,
                         alloc_numa, NULL,
                         numa_move_pages_wrapper,
                         free_numa,
                         NULL);
}

sicm_arena create_arena_sicm(sicm_device *src, sicm_device_list *srcs) {
    srcs->count = 1;
    srcs->devices = &src;
    return sicm_arena_create(0, 0, srcs);
}

void *alloc_sicm(const size_t size, int src, sicm_arena arena) {
    return sicm_arena_alloc(arena, size);
}

int move_sicm(void **ptrs, size_t count, int *dsts, int *statuses,
              sicm_arena arena, sicm_device *dst) {
    return sicm_arena_set_device(arena, dst);
}

void destroy_arena_sicm(sicm_arena arena) {
    sicm_arena_destroy(arena);
}

void *sicm_thread(void *ptr) {
    return thread_common(ptr,
                         create_arena_sicm,
                         alloc_sicm, NULL,
                         move_sicm,
                         NULL,
                         destroy_arena_sicm);
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
