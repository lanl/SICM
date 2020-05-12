/*
 * sicm_rdspy.c
 * Implementation of rdspy allocation site profiling
 * Brandon Kammerdiener
 * December, 2018
 */

#include "sicm_rdspy.h"
#include "sicm_tree.h"

#include <assert.h>
#include <pthread.h>
#include <stdio.h>

int get_thread_index();

typedef void *addr_t;
use_tree(addr_t, unsigned);

static int               max_site         = 0;
static void             *chunks_end       = 0;
static ThreadReadsInfo **tris             = NULL;
static int               n_tris           = 0;
static int               list_flush_limit = 256 * 1024;
static int               list_flush_early = 4 * 1024;
static int               max_threads;
static int               num_static_sites;

static pthread_rwlock_t       chunks_end_lock;
 
static SiteReadsAgg           agg_hist;
static pthread_mutex_t        sra_lock;

static pthread_mutex_t        tri_lock;
static pthread_key_t          tri_key;

static tree(addr_t, unsigned) site_map;
static pthread_rwlock_t       site_map_lock;

void SiteReadsAgg_init(SiteReadsAgg * sra) {
    sra->histograms = calloc(num_static_sites + 1, sizeof(hist_t));
}

void SiteReadsAgg_finish(SiteReadsAgg * sra) {
    FILE *f;
    int   b,
          next_b,
          sid;
    
    f = fopen("read_times.csv", "w");

    fprintf(f, "site");
    for (b = 0; b < READ_TIMES_MAX - READ_TIMES_BUCKET_SIZE; b += READ_TIMES_BUCKET_SIZE) {
        next_b = b + READ_TIMES_BUCKET_SIZE;
        fprintf(f, ", %d - %d", b, next_b);
    }
    fprintf(f, ", %d+\n", READ_TIMES_MAX - READ_TIMES_BUCKET_SIZE);

    for (sid = 0; sid <= num_static_sites; sid += 1) {
        fprintf(f, "%d", sid);
        for (b = 0; b < READ_TIMES_NBUCKETS; b += 1) {
            fprintf(f, ", %llu", sra->histograms[sid][b]);
        }
        fprintf(f, "\n");
    }

    fclose(f);

    free(sra->histograms);
}

void SiteReadsAgg_give_histogram(SiteReadsAgg * sra, ThreadReadsInfo * tri) {
    int sid,
        b;

    for (sid = 0; sid <= num_static_sites; sid += 1) {
        for (b = 0; b < READ_TIMES_NBUCKETS; b += 1) {
            sra->histograms[sid][b] += tri->histograms[sid][b];
        }
    }
}

void ThreadReadsInfo_init(ThreadReadsInfo * tri) {
    tri->top = tri->list = malloc(sizeof(AddrTicks) * list_flush_limit);
    tri->histograms      = calloc(num_static_sites + 1, sizeof(hist_t));
}

void ThreadReadsInfo_finish(ThreadReadsInfo * tri) {
    int i;

    for (i = 0; i < max_threads; i += 1) {
        if (tris[i] == tri) {
            pthread_rwlock_rdlock(&site_map_lock);
            ThreadReadsInfo_flush(tri);
            pthread_rwlock_unlock(&site_map_lock);

            pthread_mutex_lock(&sra_lock);
            SiteReadsAgg_give_histogram(&agg_hist, tri);
            pthread_mutex_unlock(&sra_lock);

            free(tri->list);
            free(tri->histograms);
            free(tri);
            tris[i] = NULL;
            return;
        }
    }
}

inline void ThreadReadsInfo_add(ThreadReadsInfo *tri, void *addr, uint64_t ticks) {
    AddrTicks *at;
    uint32_t len;

    at = tri->top++;
    at->addr = addr; at->ticks = ticks;

    len = tri->top - tri->list;

    if (len == list_flush_limit) {
        pthread_rwlock_rdlock(&site_map_lock);
        ThreadReadsInfo_flush(tri);
        pthread_rwlock_unlock(&site_map_lock);
    } else if (len >= list_flush_early) {
        /* if we can get the lock, go ahead and flush */
        if (pthread_rwlock_tryrdlock(&site_map_lock) == 0) {
            ThreadReadsInfo_flush(tri);
            pthread_rwlock_unlock(&site_map_lock);
        }
    }
}

inline void ThreadReadsInfo_flush(ThreadReadsInfo *tri) {
    AddrTicks                *at;
    tree_it(addr_t, unsigned) it;
    unsigned                  site,
                              bucket;
    void                     *addr;
    uint64_t                  ticks;
    
    for (at = tri->list; at <= tri->top; at++) {
        addr = at->addr;

        it = tree_gtr(site_map, addr);
        tree_it_prev(it);

        if (!tree_it_good(it)) {
            continue;
        }

        site   = tree_it_val(it);
        ticks  = at->ticks;
        bucket = ticks > READ_TIMES_MAX
                     ? READ_TIMES_NBUCKETS - 1
                     : ticks >> (READ_TIMES_BUCKET_SIZE / 2);

        tri->histograms[site][bucket] += 1;
    }

    tri->top = tri->list;
}

static ThreadReadsInfo * get_tri() {
    ThreadReadsInfo *tri;
    int              idx;
    
    tri = pthread_getspecific(tri_key);

    if (tri == NULL) {
        pthread_mutex_lock(&tri_lock);
        idx = n_tris++;
        tri = (ThreadReadsInfo*)malloc(sizeof(ThreadReadsInfo));
        pthread_setspecific(tri_key, tri);
        ThreadReadsInfo_init(tri);
        tris[idx] = tri;
        pthread_mutex_unlock(&tri_lock);
    }

    return tri;
}

void sh_rdspy_alloc(void *ptr, size_t sz, unsigned id) {
      pthread_rwlock_wrlock(&chunks_end_lock);
      if (ptr + sz > chunks_end) {
          chunks_end = ptr + sz;
      }
      pthread_rwlock_unlock(&chunks_end_lock);

      pthread_rwlock_wrlock(&site_map_lock);
      tree_insert(site_map, ptr, id);
      pthread_rwlock_unlock(&site_map_lock);
}

void sh_rdspy_realloc(void *old, void *ptr, size_t sz, unsigned id) {
      pthread_rwlock_wrlock(&chunks_end_lock);
      if (ptr + sz > chunks_end)
          chunks_end = ptr + sz;
      pthread_rwlock_unlock(&chunks_end_lock);

      if (ptr != old) {
          pthread_rwlock_wrlock(&site_map_lock);
          /* tree_delete(site_map, old); */
          tree_insert(site_map, ptr, id);
          pthread_rwlock_unlock(&site_map_lock);
      }
}

void sh_rdspy_free(void *ptr) {
    int              i;
    ThreadReadsInfo *tri;

    tri = get_tri();

    pthread_mutex_lock(&tri_lock); 

    pthread_rwlock_rdlock(&site_map_lock);
    ThreadReadsInfo_flush(tris[i]);
    pthread_rwlock_unlock(&site_map_lock);

    /* for (i = 0; i < n_tris; i += 1) { */
    /*     pthread_rwlock_rdlock(&site_map_lock); */
    /*     ThreadReadsInfo_flush(tris[i]); */
    /*     pthread_rwlock_unlock(&site_map_lock); */
    /* } */
    
    pthread_mutex_unlock(&tri_lock); 
}

unsigned long long sh_read(void * ptr, uint64_t beg, uint64_t end) {
    ThreadReadsInfo *tri;

    if (ptr >= chunks_end) {
        return 0;
    }

    tri = get_tri();

    ThreadReadsInfo_add(tri, ptr, end - beg);

    return 1;
}

void sh_rdspy_init(int _max_threads, int _num_static_sites) {
    max_threads      = _max_threads;
    num_static_sites = _num_static_sites;

    site_map  = tree_make(addr_t, unsigned);
    SiteReadsAgg_init(&agg_hist);
    tris = malloc(sizeof(ThreadReadsInfo*) * max_threads);
    memset(tris, 0, sizeof(ThreadReadsInfo*) * max_threads);

    pthread_rwlock_init(&chunks_end_lock, NULL);
    pthread_mutex_init(&sra_lock, NULL);
    pthread_mutex_init(&tri_lock, NULL);
    pthread_key_create(&tri_key, (void(*)(void*))ThreadReadsInfo_finish);
    pthread_rwlock_init(&site_map_lock, NULL);
}

void sh_rdspy_terminate() {
    int i;

    for (i = 0; i < n_tris; i += 1) {
        if (tris[i]) {
            ThreadReadsInfo_finish(tris[i]);
        }
    }
    SiteReadsAgg_finish(&agg_hist);
}
