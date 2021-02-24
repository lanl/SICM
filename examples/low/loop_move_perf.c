#include <errno.h>
#include <numa.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/mman.h>

#include "nano.h"
#include "sicm_low.h"

struct ThreadArgs {
    sicm_device_list *devs;
    sicm_device *src;
    sicm_device *dst;
    size_t allocations;
    size_t *sizes;
};

void *thread_common(struct ThreadArgs *args,
                    void (*ALLOC)(void **, const size_t, const size_t, sicm_device *),
                    void (*MOVE) (void *,  const size_t, sicm_device *, sicm_device *),
                    void (*FREE) (void *,  const size_t, sicm_device *)) {
    const size_t size = args->allocations * sizeof(void *);
    void **ptrs = numa_alloc_onnode(size, 0);
    memset(ptrs, 0, size);

    struct timespec start, end;
    clock_gettime(CLOCK_MONOTONIC, &start);

    /* allocate */
    for(size_t i = 0; i < args->allocations; i++) {
        ALLOC(ptrs, i, args->sizes[i], args->src);
        if (!ptrs[i]) {
            fprintf(stderr, "Could not allocate ptrs[%zu]\n", i);
        }
    }

    /* move */
    for(size_t i = 0; i < args->allocations; i++) {
        MOVE(ptrs[i], args->sizes[i], args->src, args->dst);
    }

    /* free */
    for(size_t i = 0; i < args->allocations; i++) {
        FREE(ptrs[i], args->sizes[i], args->dst);
        ptrs[i] = NULL;
    }

    clock_gettime(CLOCK_MONOTONIC, &end);
    double *elapsed = numa_alloc_onnode(sizeof(double), 0);
    *elapsed = nano(&start, &end);

    numa_free(ptrs, size);

    return elapsed;
}

void alloc_mmap(void **ptrs, const size_t i, const size_t size, sicm_device *src) {
    int status = 0;
    ptrs[i] = mmap(NULL, size, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    if (numa_move_pages(0, 1, &ptrs[i], &src->node, &status, 0) < 0) {
        const int err = errno;
        fprintf(stderr, "Could not move ptrs[%d]=%p to %d: %d %s\n",
                i, ptrs[i], src->node, err, strerror(err));
        return;
    }
}

void move_mmap(void *ptr, const size_t size, sicm_device *src, sicm_device *dst) {
    int status = 0;
    if (numa_move_pages(0, 1, &ptr, &dst->node, &status, 0) < 0) {
        const int err = errno;
        fprintf(stderr, "Could not move %p to %d: %d %s\n",
                ptr, dst->node, err, strerror(err));
    }
}

void free_mmap(void *ptr, const size_t size, sicm_device *location) {
    munmap(ptr, size);
}

void *mmap_thread(void *ptr) {
    return thread_common(ptr, alloc_mmap, move_mmap, free_mmap);
}

void alloc_numa(void **ptrs, const size_t i, const size_t size, sicm_device *src) {
    ptrs[i] = numa_alloc_onnode(size, src->node);
}

void move_numa(void *ptr, const size_t size, sicm_device *src, sicm_device *dst) {
    int status = 0;
    if (numa_move_pages(0, 1, &ptr, &dst->node, &status, 0) < 0) {
        const int err = errno;
        fprintf(stderr, "Could not move %p to %d: %d %s\n",
                ptr, dst->node, err, strerror(err));
        return;
    }
}

void free_numa(void *ptr, const size_t size, sicm_device *location) {
    numa_free(ptr, size);
}

void *numa_thread(void *ptr) {
    return thread_common(ptr, alloc_numa, move_numa, free_numa);
}

void alloc_sicm(void **ptrs, const size_t i, const size_t size, sicm_device *src) {
    ptrs[i] = sicm_device_alloc(src, size);
}

void move_sicm(void *ptr, const size_t size, sicm_device *src, sicm_device *dst) {
    sicm_move(src, dst, ptr, size);
}

void free_sicm(void *ptr, const size_t size, sicm_device *location) {
    sicm_device_free(location, ptr, size);
}

void *sicm_thread(void *ptr) {
    return thread_common(ptr, alloc_sicm, move_sicm, free_sicm);
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
                elapsed += * (double *) thread_elapsed;
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
    srand(time(NULL));

    if (argc < 4) {
        fprintf(stderr, "Syntax: %s max_size thread_count allocation_count\n", argv[0]);
        return 1;
    }

    size_t max_size = 0;
    if ((sscanf(argv[1], "%zu", &max_size) != 1) || !max_size) {
        fprintf(stderr, "Bad max size: %s\n", argv[1]);
        return 1;
    }

    size_t thread_count = 0;
    if (sscanf(argv[2], "%zu", &thread_count) != 1) {
        fprintf(stderr, "Bad thread count: %s\n", argv[2]);
        return 1;
    }

    size_t allocations = 0;
    if (sscanf(argv[3], "%zu", &allocations) != 1) {
        fprintf(stderr, "Bad allocation count: %s\n", argv[3]);
        return 1;
    }

    /* printf("Allocating %zu randomly sized buffers on %zu threads each:\n", allocations, thread_count); */

    size_t *sizes = calloc(allocations, sizeof(size_t));
    for(size_t i = 0; i < allocations; i++) {
        while ((sizes[i] = (rand() * rand() * rand()) % max_size) == 0);
        /* printf("    %zu %zu\n", i, sizes[i]); */
    }

    sicm_device_list devs = sicm_init();

    printf("Found %u NUMA nodes:\n", devs.count / 3);
    for(size_t i = 0; i < devs.count; i += 3) {
        printf("    %d %s\n", devs.devices[i]->node,
               sicm_device_tag_str(devs.devices[i]->tag));
    }

    printf("%10s        %12s   %12s\n", "", "RealTime", "ThreadTime");
    run(&devs, thread_count, allocations, sizes, "mmap", mmap_thread);
    run(&devs, thread_count, allocations, sizes, "numa", numa_thread);
    run(&devs, thread_count, allocations, sizes, "sicm", sicm_thread);

    sicm_fini();

    free(sizes);

    return 0;
}
