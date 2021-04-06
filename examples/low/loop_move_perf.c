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

void *thread_common(struct ThreadArgs *args,
                    void (*ALLOC)(void **, const size_t, const size_t, sicm_device *),
                    void (*MOVE) (void *,  const size_t, sicm_device *, sicm_device *),
                    void (*FREE) (void *,  const size_t, sicm_device *)) {
    if (pin_thread(args->id) != 0) {
        fprintf(stderr, "Could not pin thread to cpu %zu\n", args->id);
        return NULL;
    }

    const size_t size = args->allocations * sizeof(void *);
    void **ptrs = numa_alloc_onnode(size, 0);
    memset(ptrs, 0, size);

    /* allocate */
    for(size_t i = 0; i < args->allocations; i++) {
        ALLOC(ptrs, i, args->sizes[i], args->src);

        if (!ptrs[i]) {
            fprintf(stderr, "Could not allocate ptrs[%zu]\n", i);
        }

        memset(ptrs[i], 0, args->sizes[i]);
    }

    clock_gettime(CLOCK_MONOTONIC, &args->start);

    /* move */
    for(size_t i = 0; i < args->allocations; i++) {
        MOVE(ptrs[i], args->sizes[i], args->src, args->dst);
    }

    clock_gettime(CLOCK_MONOTONIC, &args->end);

    /* free */
    for(size_t i = 0; i < args->allocations; i++) {
        FREE(ptrs[i], args->sizes[i], args->dst);
        ptrs[i] = NULL;
    }

    numa_free(ptrs, size);

    return NULL;
}

void alloc_malloc(void **ptrs, const size_t i, const size_t size, sicm_device *src) {
    int status = 0;
    ptrs[i] = malloc(size);
    if (numa_move_pages(0, 1, &ptrs[i], &src->node, &status, 0) < 0) {
        const int err = errno;
        fprintf(stderr, "Could not move ptrs[%zu]=%p to %d: %d %s\n",
                i, ptrs[i], src->node, err, strerror(err));
        return;
    }
}

void move_malloc(void *ptr, const size_t size, sicm_device *src, sicm_device *dst) {
    int status = 0;
    if (numa_move_pages(0, 1, &ptr, &dst->node, &status, 0) < 0) {
        const int err = errno;
        fprintf(stderr, "Could not move %p to %d: %d %s\n",
                ptr, dst->node, err, strerror(err));
    }
}

void free_malloc(void *ptr, const size_t size, sicm_device *location) {
    free(ptr);
}

void *malloc_thread(void *ptr) {
    return thread_common(ptr, alloc_malloc, move_malloc, free_malloc);
}

void alloc_mmap(void **ptrs, const size_t i, const size_t size, sicm_device *src) {
    int status = 0;
    ptrs[i] = mmap(NULL, size, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    if (numa_move_pages(0, 1, &ptrs[i], &src->node, &status, 0) < 0) {
        const int err = errno;
        fprintf(stderr, "Could not move ptrs[%zu]=%p to %d: %d %s\n",
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
