#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif

#include <pthread.h>
#include <stdio.h>
#include <time.h>

#include "nano.h"
#include "run_move.h"

int pin_thread(size_t cpu) {
    cpu_set_t cpuset;
    CPU_ZERO(&cpuset);
    CPU_SET(cpu, &cpuset);
    return pthread_setaffinity_np(pthread_self(), sizeof(cpu_set_t), &cpuset);
}

void run(sicm_device_list *devs,
         const size_t thread_count,
         const size_t allocations,
         size_t *sizes,
         size_t reps,
         const char *name,
         void *(*func)(void *)) {
    /* use the same arrays for all runs */
    pthread_t *threads      = calloc(thread_count, sizeof(pthread_t));
    struct ThreadArgs *args = calloc(thread_count, sizeof(struct ThreadArgs));

    /* move from all NUMA nodes to all NUMA nodes */
    for(int dst_idx = 0; dst_idx < devs->count; dst_idx += 3) {
        printf("%d", devs->devices[dst_idx]->node);
        for(int src_idx = 0; src_idx < devs->count; src_idx += 3) {
            double total_threadtime = 0;
            double total_realtime = 0;

            for(size_t i = 0; i < thread_count; i++) {
                args[i].id = i;
                args[i].devs = devs;
                args[i].src = devs->devices[src_idx];
                args[i].dst = devs->devices[dst_idx];
                args[i].allocations = allocations;
                args[i].sizes = sizes;
            }

            for(size_t iter = 0; iter < reps; iter++) {
                struct timespec start, end;
                clock_gettime(CLOCK_MONOTONIC, &start);
                for(size_t i = 0; i < thread_count; i++) {
                    if (pthread_create(&threads[i], NULL, func, &args[i]) != 0) {
                        fprintf(stderr, "Could not create thread %d of %s\n", i, name);
                        for(int j = 0; j < thread_count; j++) {
                            pthread_join(threads[j], NULL);
                            threads[j] = 0;
                        }
                        break;
                    }
                }

                for(size_t i = 0; i < thread_count; i++) {
                    pthread_join(threads[i], NULL);
                }
                clock_gettime(CLOCK_MONOTONIC, &end);

                double elapsed = 0;
                for(size_t i = 0; i < thread_count; i++) {
                    elapsed += nano(&args[i].start, &args[i].end);
                }

                total_threadtime += elapsed;
                total_realtime += nano(&start, &end);
            }

            /* /\* print average real time *\/ */
            /* printf(" %.3f", (total_realtime / reps) / 1e9); */

            printf(" %.3f", (total_threadtime / reps) / 1e9);
            fflush(stdout);
        }

        printf("\n");
    }

    free(args);
    free(threads);
}
