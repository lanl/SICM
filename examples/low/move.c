#include <errno.h>
#include <numa.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/mman.h>

#include "sicm_low.h"
#include "sizes.h"

/*
  This C program is not really meant to run. Rather, it is meant to
  demonstrate the differences between using mmap, numa, and sicm.
*/

/*
   malloc is simple
   Allocating to the same device requires allocation policy modifications
   No guarantee that allocations are located close to each other
*/
void **malloc_create(size_t *sizes, size_t count, int node) {
    void **ptrs = malloc(count * sizeof(void *));
    for(size_t i = 0; i < count; i++) {
        ptrs[i] = malloc(sizes[i]);
    }

    return ptrs;
}

/*
   Freeing is simple
*/
void malloc_destroy(void **ptrs, size_t count) {
    for(size_t i = 0; i < count; i++) {
        free(ptrs[i]);
    }

    free(ptrs);
}

void malloc_example(size_t *sizes, size_t count, int src, int dst) {
    void **ptrs = malloc_create(sizes, count, src);

    /*
      Moving malloc-ed pointers using an array of destinations
      requires 2 extra arrays to be allocated
    */
    {
        int *dsts     = malloc(count * sizeof(int));
        int *statuses = malloc(count * sizeof(int));

        for(size_t i = 0; i < count; i++) {
            dsts[i] = dst;
            statuses[i] = -1;
        }

        numa_move_pages(0, count, ptrs, dsts, statuses, 0);

        free(dsts);
        free(statuses);
    }

    /*
      Moving malloc-ed pointers using a loop requires walking through each
      pointer
    */
    {
        int status = -1;
        for(size_t i = 0; i < count; i++) {
            numa_move_pages(0, 1, &ptrs[i], &dst, &status, 0);
        }
    }

    malloc_destroy(ptrs, count);
}

/*
   mmap arguments are awkward
   Allocating to the same device requires allocation policy modifications
   No guarantee that allocations are located close to each other
*/
void **mmap_create(size_t *sizes, size_t count, int node) {
    void **ptrs = malloc(count * sizeof(void *));
    for(size_t i = 0; i < count; i++) {
        ptrs[i] = mmap(NULL, sizes[i], PROT_READ | PROT_WRITE,
                       MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    }

    return ptrs;
}

/*
   Freeing requires the size as well as the pointer
*/
void mmap_destroy(void **ptrs, size_t *sizes, size_t count) {
    for(size_t i = 0; i < count; i++) {
        munmap(ptrs[i], sizes[i]);
    }

    free(ptrs);
}

void mmap_example(size_t *sizes, size_t count, int src, int dst) {
    void **ptrs = mmap_create(sizes, count, src);

    /*
      Moving mmap-ed pointers using an array of destinations
      requires 2 extra arrays to be allocated
    */
    {
        int *dsts     = malloc(count * sizeof(int));
        int *statuses = malloc(count * sizeof(int));

        for(size_t i = 0; i < count; i++) {
            dsts[i] = dst;
            statuses[i] = -1;
        }

        numa_move_pages(0, count, ptrs, dsts, statuses, 0);

        free(dsts);
        free(statuses);
    }

    /*
      Moving mmap-ed pointers using a loop requires walking through each
      pointer
    */
    {
        int status = -1;
        for(size_t i = 0; i < count; i++) {
            numa_move_pages(0, 1, &ptrs[i], &dst, &status, 0);
        }
    }

    mmap_destroy(ptrs, sizes, count);
}

/**************************************************************************/

/*
   numa allocation on a numa node is easy
   However, there is no guarantee that the pointers are located close to each other
*/
void **numa_create(size_t *sizes, size_t count, int src) {
    void **ptrs = malloc(count * sizeof(void *));
    for(size_t i = 0; i < count; i++) {
        ptrs[i] = numa_alloc_onnode(sizes[i], src);
    }

    return ptrs;
}

/*
   Freeing requires the size as well as the pointer
*/
void numa_destroy(void **ptrs, size_t *sizes, size_t count) {
    for(size_t i = 0; i < count; i++) {
        numa_free(ptrs[i], sizes[i]);
    }

    free(ptrs);
}

void numa_example(size_t *sizes, size_t count, int src, int dst) {
    void **ptrs = numa_create(sizes, count, src);

    /*
      Moving numa pointers using an array of destinations
      requires 2 extra arrays to be allocated
    */
    {
        int *dsts     = malloc(count * sizeof(int));
        int *statuses = malloc(count * sizeof(int));

        for(size_t i = 0; i < count; i++) {
            dsts[i] = dst;
            statuses[i] = -1;
        }

        numa_move_pages(0, count, ptrs, dsts, statuses, 0);

        free(dsts);
        free(statuses);
    }

    /*
      Moving numa-ed pointers using a loop requires walking through each
      pointer
    */
    {
        int status = -1;
        for(size_t i = 0; i < count; i++) {
            numa_move_pages(0, 1, &ptrs[i], &dst, &status, 0);
        }
    }
    numa_destroy(ptrs, sizes, count);
}

/**************************************************************************/

/*
   Allocating on the same device/NUMA node simply requires providing a
   device or device list

   Allocations are guaranteed to be either close to each other or
   associated with each other (pages backing the same arena)

   Pointers do not need to be saved by the caller - the arena is all
   that is necessary Although it seems that the arena is simply a
   replacement variable for the ptrs array in the other examples, it
   is likely not because arenas need to be created to force allocations
   to be made close to each other.
*/
sicm_arena sicm_create(sicm_device_list *devs, size_t *sizes, size_t count, sicm_device *src) {
    sicm_arena arena = sicm_arena_create(0, 0, devs);

    for(size_t i = 0; i < count; i++) {
        /* ptrs[i] = */ sicm_arena_alloc(arena, sizes[i]);
    }

    return arena;
}

/*
  all pointers on an arena can be deallcated in one call

  if the pointers are available, the arena can be looked up
*/
void sicm_destroy(sicm_arena arena) {
    sicm_arena_destroy(arena);
}

/*
  moving SICM pointers requires only that all the pointers start on
  the same arena
 */
void sicm_example(sicm_device_list *devs, size_t *sizes, size_t count, sicm_device *src, sicm_device *dst) {
    sicm_arena arena = sicm_create(devs, sizes, count, src);
    sicm_arena_set_device(arena, dst);
    sicm_destroy(arena);
}

int main(int argc, char *argv[]) {
    if (argc < 3) {
        fprintf(stderr, "Syntax: %s max_size allocation_count\n", argv[0]);
        return 1;
    }

    size_t max_size = 0;
    if ((sscanf(argv[1], "%zu", &max_size) != 1) || !max_size) {
        fprintf(stderr, "Bad max size: %s\n", argv[1]);
        return 1;
    }

    size_t allocations = 0;
    if (sscanf(argv[2], "%zu", &allocations) != 1) {
        fprintf(stderr, "Bad allocation count: %s\n", argv[2]);
        return 1;
    }

    sicm_device_list devs = sicm_init();

    /* randomly generate sizes to represent allocations of some process */
    size_t *sizes = select_sizes(max_size, allocations);

    malloc_example(       sizes, allocations, devs.devices[0]->node, devs.devices[3]->node);
    mmap_example  (       sizes, allocations, devs.devices[0]->node, devs.devices[3]->node);
    numa_example  (       sizes, allocations, devs.devices[0]->node, devs.devices[3]->node);
    sicm_example  (&devs, sizes, allocations, devs.devices[0],       devs.devices[3]);

    free(sizes);

    sicm_fini();

    return 0;
}
