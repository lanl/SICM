#define _GNU_SOURCE
#include <numa.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <time.h>

#include <kmeans/kmeans.h>

/* diff two timespecs */
long double nanoseconds(struct timespec * start, struct timespec * end) {
    long double s = (long double) start->tv_nsec;
    s /= 1e9;
    s += (long double) start->tv_sec;
    long double e = (long double) end->tv_nsec;
    e /= 1e9;
    e += (long double) end->tv_sec;

    return e - s;
}

/* Fisher-Yates shuffle */
size_t * shuffle_array(size_t * array, const size_t size) {
    for(size_t i = size - 1; i > 0; i--) {
        const size_t j = rand() % (i + 1);
        array[i] ^= array[j];
        array[j] ^= array[i];
        array[i] ^= array[j];
    };

    return array;
}

/* sequential read */
long double sequential_read(int * array, const size_t size) {
    struct timespec start, end;
    clock_gettime(CLOCK_MONOTONIC, &start);
    #pragma omp parallel for
    for(size_t i = 0; i < size; i++) {
        (void) array[i];
    }
    clock_gettime(CLOCK_MONOTONIC, &end);
    return nanoseconds(&start, &end);
}

/* sequential write */
long double sequential_write(int * array, const size_t size, const int value) {
    struct timespec start, end;
    clock_gettime(CLOCK_MONOTONIC, &start);
    #pragma omp parallel for
    for(size_t i = 0; i < size; i++) {
        array[i] = value;
    }
    clock_gettime(CLOCK_MONOTONIC, &end);
    return nanoseconds(&start, &end);
}

/* sequential read-write */
long double sequential_read_write(int * lhs, int * rhs, const size_t size) {
    struct timespec start, end;
    clock_gettime(CLOCK_MONOTONIC, &start);
    #pragma omp parallel for
    for(size_t i = 0; i < size; i++) {
        lhs[i] += rhs[i];
    }
    clock_gettime(CLOCK_MONOTONIC, &end);
    return nanoseconds(&start, &end);
}

/* random read */
long double random_read(int * array, const size_t size, size_t * order) {
    struct timespec start, end;
    clock_gettime(CLOCK_MONOTONIC, &start);
    #pragma omp parallel for
    for(size_t i = 0; i < size; i++) {
        (void) array[order[i]];
    }
    clock_gettime(CLOCK_MONOTONIC, &end);
    return nanoseconds(&start, &end);
}

/* random write */
long double random_write(int * array, const size_t size, const int value, size_t * order) {
    struct timespec start, end;
    clock_gettime(CLOCK_MONOTONIC, &start);
    #pragma omp parallel for
    for(size_t i = 0; i < size; i++) {
        array[order[i]] = value;
    }
    clock_gettime(CLOCK_MONOTONIC, &end);
    return nanoseconds(&start, &end);
}

/* random read-write */
long double random_read_write(int * lhs, int * rhs, const size_t size, size_t * order) {
    struct timespec start, end;
    clock_gettime(CLOCK_MONOTONIC, &start);
    #pragma omp parallel for
    for(size_t i = 0; i < size; i++) {
        const size_t j = order[i];
        lhs[j] += rhs[j];
    }
    clock_gettime(CLOCK_MONOTONIC, &end);
    return nanoseconds(&start, &end);
}

/* mirror read */
long double mirror_read(int * array, const size_t size) {
    struct timespec start, end;
    clock_gettime(CLOCK_MONOTONIC, &start);
    #pragma omp parallel for
    for(size_t i = 0; i < size; i++) {
        (void) array[i];
        (void) array[size - 1 - i];
    }
    clock_gettime(CLOCK_MONOTONIC, &end);
    return nanoseconds(&start, &end);
}

/* mirror write */
long double mirror_write(int * array, const size_t size, const int value) {
    struct timespec start, end;
    clock_gettime(CLOCK_MONOTONIC, &start);
    #pragma omp parallel for
    for(size_t i = 0; i < size; i++) {
        array[i] = value;
        array[size - 1 - i] = value;
    }
    clock_gettime(CLOCK_MONOTONIC, &end);
    return nanoseconds(&start, &end);
}

/* mirror read write */
long double mirror_read_write(int * array, const size_t size) {
    struct timespec start, end;
    clock_gettime(CLOCK_MONOTONIC, &start);
    #pragma omp parallel for
    for(size_t i = 0; i < size; i++) {
        array[i] = array[size - 1 - i];
        array[size - 1 - i] = array[i];
    }
    clock_gettime(CLOCK_MONOTONIC, &end);
    return nanoseconds(&start, &end);
}

#define rwrw(name)  \
    long double name##_read; \
    long double name##_write; \
    long double name##_read_write; \

struct stats {
    int node;
    long long size;
    rwrw(sequential);
    rwrw(random);
    rwrw(mirror);
    long double avg;
};

double distance(const Pointer lhs, const Pointer rhs) {
    struct stats * l = (struct stats *) lhs;
    struct stats * r = (struct stats *) rhs;

    const long double sdr  = l->sequential_read - r->sequential_read;
    const long double sdw  = l->sequential_write - r->sequential_write;
    const long double sdrw = l->sequential_read_write - r->sequential_read_write;
    const long double rdr  = l->random_read  - r->random_read;
    const long double rdw  = l->random_write - r->random_write;
    const long double rdrw = l->random_read_write - r->random_read_write;
    const long double mdr  = l->mirror_read  - r->mirror_read;
    const long double mdw  = l->mirror_write - r->mirror_write;
    const long double mdrw = l->mirror_read_write - r->mirror_read_write;
    const long double davg = l->avg - r->avg;

    return sdr * sdr +
           sdw * sdw +
           sdrw * sdrw +
           rdr * rdr +
           rdw * rdw +
           rdrw * rdrw +
           mdr * mdr +
           mdw * mdw +
           mdrw * mdrw +
           davg * davg +
           0;
}

/* from kmeans/example2.c */
void centroid(const Pointer * objs, const int * clusters, size_t num_objs, int cluster, Pointer centroid)
{
    int num_cluster = 0;
    struct stats sum;
    struct stats **pts = (struct stats**)objs;
    struct stats *center = (struct stats*)centroid;

    memset(&sum, 0, sizeof(struct stats));

    if (num_objs <= 0) return;

    for (int i = 0; i < num_objs; i++)
    {
        /* Only process objects of interest */
        if (clusters[i] != cluster) continue;

        sum.sequential_read += pts[i]->sequential_read;
        sum.sequential_write += pts[i]->sequential_write;
        sum.sequential_read_write += pts[i]->sequential_read_write;
        sum.random_read += pts[i]->random_read;
        sum.random_write += pts[i]->random_write;
        sum.random_read_write += pts[i]->random_read_write;
        sum.mirror_read += pts[i]->mirror_read;
        sum.mirror_write += pts[i]->mirror_write;
        sum.mirror_read_write += pts[i]->mirror_read_write;
        sum.avg += pts[i]->avg;

        num_cluster++;
    }
    if (num_cluster)
    {
        sum.sequential_read /= num_cluster;
        sum.sequential_write /= num_cluster;
        sum.sequential_read_write /= num_cluster;
        sum.random_read /= num_cluster;
        sum.random_write /= num_cluster;
        sum.random_read_write /= num_cluster;
        sum.mirror_read /= num_cluster;
        sum.mirror_write /= num_cluster;
        sum.mirror_read_write /= num_cluster;
        sum.avg /= num_cluster;
        *center = sum;
    }
    return;
}

struct stats * run_kernels(int * numa_nodes, const size_t numa_node_count,
                           const size_t size, const size_t iterations) {
    /* number of elements */
    const size_t count = size / sizeof(int);

    /* an array of indicies */
    size_t * order = calloc(count, sizeof(size_t));
    for(size_t i = 0; i < count; i++) {
        order[i] = i;
    }

    struct stats * stats = calloc(numa_node_count, sizeof(struct stats));

    /* run kernels on each numa node */
    for(size_t i = 0; i < numa_node_count; i++) {
        stats[i].node = numa_nodes[i];
        stats[i].size = numa_node_size64(stats[i].node, NULL);

        /* run each kernel multiple times */
        for(size_t j = 0; j < iterations; j++) {
            int * lhs = numa_alloc_onnode(size, numa_nodes[i]);
            int * rhs = numa_alloc_onnode(size, numa_nodes[i]);

            memset(lhs, 0, size);
            memset(rhs, 0, size);

            /* /\* change the order indicies are read from *\/ */
            /* shuffle_array(order, count); */

            /* run each kernel */
            stats[i].sequential_read       += sequential_read           (rhs, count);
            stats[i].sequential_write      += sequential_write          (rhs, count, 0);
            stats[i].sequential_read_write += sequential_read_write(lhs, rhs, count);
            stats[i].random_read           += random_read               (rhs, count,    shuffle_array(order, count));
            stats[i].random_write          += random_write              (rhs, count, 0, shuffle_array(order, count));
            stats[i].random_read_write     += random_read_write    (lhs, rhs, count,    shuffle_array(order, count));
            stats[i].mirror_read           += mirror_read               (rhs, count);
            stats[i].mirror_write          += mirror_write              (rhs, count, 0);
            stats[i].mirror_read_write     += mirror_read_write         (rhs, count);

            numa_free(lhs, size);
            numa_free(rhs, size);
        }

        stats[i].avg = (stats[i].sequential_read +
                        stats[i].sequential_write +
                        stats[i].sequential_read_write +
                        stats[i].random_read +
                        stats[i].random_write +
                        stats[i].random_read_write +
                        stats[i].mirror_read +
                        stats[i].mirror_write +
                        stats[i].mirror_read_write) / 6.0;
    }

    free(order);

    return stats;
}

/* sorts backwards; highest time comes first */
int sort_by_avg_of_avg(const void * lhs, const void * rhs) {
    size_t l_count = 0;
    long double l_sum = 0;
    for(struct stats ** l = *(struct stats ***) lhs; *l; l++) {
        l_sum += (*l)->avg;
        l_count++;
    }

    size_t r_count = 0;
    long double r_sum = 0;
    for(struct stats ** r = *(struct stats ***) rhs; *r; r++) {
        r_sum += (*r)->avg;
        r_count++;
    }

    const long double l_avg = l_sum / l_count;
    const long double r_avg = r_sum / r_count;
    if (l_avg < r_avg) {
        return -1;
    }
    else if (l_avg > r_avg) {
        return 1;
    }

    return 0;
}

int main(int argc, char * argv[]) {
    srand(time(NULL));

    if (argc < 4) {
        fprintf(stderr, "%s size iterations memory_type [memory_type ...]\n", argv[0]);
        fprintf(stderr, "\n");
        fprintf(stderr, "The order memory types should be listed, if available:\n");
        fprintf(stderr, "    DRAM HBM GPU OPTANE\n\n");
        return 1;
    }

    size_t size = 0;
    if (sscanf(argv[1], "%zu", &size) != 1) {
        fprintf(stderr, "Bad memory size\n");
        return 1;
    }

    size_t iterations = 0;
    if (sscanf(argv[2], "%zu", &iterations) != 1) {
        fprintf(stderr, "Bad iteration count\n");
        return 1;
    }

    if (numa_available() == -1) {
        fprintf(stderr, "could not start libnuma\n");
        return 1;
    }

    /* find numa nodes */
    const int max_numa_nodes = numa_max_node() + 1;
    size_t numa_node_count = 0;
    int * numa_nodes = calloc(max_numa_nodes, sizeof(int));
    for(int i = 0; i < max_numa_nodes; i++) {
        if (numa_bitmask_isbitset(numa_all_nodes_ptr, i)) {
            numa_nodes[numa_node_count++] = i;
        }
    }

    /* get numbers from accessing NUMA nodes */
    struct stats * stats = run_kernels(numa_nodes, numa_node_count, size, iterations);

    #if defined(DEBUG) || defined(KERNEL)
    for(size_t i = 0; i < numa_node_count; i++) {
        fprintf(stdout, "%d %Lf %Lf %Lf %Lf %Lf %Lf %Lf %Lf %Lf %Lf\n",
                stats[i].node,
                stats[i].sequential_read,
                stats[i].sequential_write,
                stats[i].sequential_read_write,
                stats[i].random_read,
                stats[i].random_write,
                stats[i].random_read_write,
                stats[i].mirror_read,
                stats[i].mirror_write,
                stats[i].mirror_read_write,
                stats[i].avg
            );
    }
    #endif

    /* cluster the results */
    kmeans_config config;
    config.k = argc - 3;
    config.num_objs = numa_node_count;
    config.max_iterations = iterations * 2;
    config.distance_method = distance;
    config.centroid_method = centroid;
    config.objs = calloc(config.num_objs, sizeof(Pointer));
    config.centers = calloc(config.k, sizeof(Pointer));
    config.clusters = calloc(config.num_objs, sizeof(Pointer));

    for(int i = 0; i < config.num_objs; i++) {
        config.objs[i] = &stats[i];
    }

    /* need to create copy of centers because kmeans function overwrites data */
    struct stats * centers_copy = calloc(config.k, sizeof(struct stats));
    for(int i = 0; i < config.k; i++) {
        centers_copy[i] = stats[i];
        config.centers[i] = &centers_copy[i];
    }

    /* figure out which stat belongs in which cluster */
    kmeans(&config);

    /* array of clusters */
    struct stats *** clustered = calloc(config.k, sizeof(struct stats **));
    for(int i = 0; i < config.k; i++) {
        #if defined(DEBUG) || defined(CLUSTERS)
        fprintf(stderr, "Cluster %d:", i);
        #endif
        /* count how many items are in cluster i */
        size_t count = 0;
        for(int j = 0; j < config.num_objs; j++) {
            if (config.clusters[j] == i) {
                count++;
            }
        }

        clustered[i] = calloc(count + 1, sizeof(struct stats *));

        /* put the items into the cluster */
        size_t k = 0;
        for(int j = 0; j < config.num_objs; j++) {
            if (config.clusters[j] == i) {
                clustered[i][k] = config.objs[j];
                #if defined(DEBUG) || defined(CLUSTERS)
                fprintf(stderr, " %d", clustered[i][k]->node);
                #endif
                k++;
            }
        }
        #if defined(DEBUG) || defined(CLUSTERS)
        fprintf(stderr, "\n");
        #endif
    }

    /* sort clusters by average of averages */
    qsort(clustered, config.k, sizeof(struct stats **), sort_by_avg_of_avg);

    /* print */
    for(int i = 0; i < config.k; i++) {
        fprintf(stdout, "%s:", argv[3 + i]);
        for(struct stats ** cluster = clustered[i]; *cluster; cluster++) {
            fprintf(stdout, " %d", (*cluster)->node);
        }
        fprintf(stdout, "\n");
        free(clustered[i]);
    }

    /* cleanup */
    free(clustered);
    free(centers_copy);
    free(config.objs);
    free(config.clusters);
    free(config.centers);
    free(stats);
    free(numa_nodes);

    return 0;
}
