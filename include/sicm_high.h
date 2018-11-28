#pragma once

#include <inttypes.h>
#include "sicm_low.h"
#include "sicm_impl.h"
#include "tree.h"

/* !!!rdspy */
#define READ_TIMES_BUCKET_SIZE (8)
#define READ_TIMES_MAX (80)
#define READ_TIMES_NBUCKETS (READ_TIMES_MAX / READ_TIMES_BUCKET_SIZE)

#include <stdint.h>
#include "tree.h"

typedef void *addr_t;
use_tree(addr_t, unsigned);
typedef struct {} empty_tree_val;
use_tree(unsigned, empty_tree_val);

typedef unsigned long long hist_t[READ_TIMES_NBUCKETS];

typedef struct {
    hist_t * histograms;
    unsigned n_histograms; 
} ThreadReadsInfo;

void ThreadReadsInfo_init(ThreadReadsInfo*);
void ThreadReadsInfo_finish(ThreadReadsInfo*);

typedef struct {
    tree(unsigned, empty_tree_val) used_sites;
    tree(addr_t, unsigned)         site_map;
    void *                         chunks_end;
    hist_t *                       histograms;
    unsigned                       n_histograms;
} SiteReadsAgg;

void SiteReadsAgg_init(SiteReadsAgg*);
void SiteReadsAgg_finish(SiteReadsAgg*);
void SiteReadsAgg_give_histogram(SiteReadsAgg*, ThreadReadsInfo*);
/* !!!end */

enum arena_layout {
  SHARED_ONE_ARENA, /* One arena between all threads */
  EXCLUSIVE_ONE_ARENA, /* One arena per thread */
  SHARED_DEVICE_ARENAS, /* One arena per device */
  EXCLUSIVE_DEVICE_ARENAS, /* One arena per device per thread */
  SHARED_SITE_ARENAS, /* One arena per allocation site */
  EXCLUSIVE_SITE_ARENAS, /* One arena per allocation site per thread */
  INVALID_LAYOUT
};

/* Keeps track of additional information about arenas for profiling */
typedef struct arena_info {
  unsigned index, id;
  sicm_arena arena;
  size_t accesses, rss, peak_rss;
} arena_info;

/* A tree associating site IDs with device pointers.
 * Sites should be bound the device that they're associated with.
 * Filled with guidance from an offline profiling run or with
 * online profiling.
 */
typedef sicm_device * deviceptr;
use_tree(unsigned, deviceptr);
use_tree(deviceptr, int);

/* So we can access these things from profile.c.
 * These variables are defined in src/high/high.c.
 */
extern extent_arr *extents;
extern extent_arr *rss_extents;
extern pthread_rwlock_t extents_lock;
extern arena_info **arenas;
extern tree(unsigned, deviceptr) site_nodes;
extern int should_profile_all, should_profile_one, should_profile_rss, should_profile_online;
extern float profile_all_rate, profile_rss_rate;
extern char *profile_one_event, *profile_all_event;
extern sicm_device *online_device;
extern sicm_device *default_device;
extern ssize_t online_device_cap;
extern int max_index;
extern int max_sample_pages;
extern int sample_freq;
extern int num_imcs, max_imc_len, max_event_len;
extern char **imcs;

#define DEFAULT_ARENA_LAYOUT INVALID_LAYOUT

__attribute__((constructor))
void sh_init();

__attribute__((destructor))
void sh_terminate();

void* sh_alloc_exact(int id, size_t sz);
void* sh_alloc(int id, size_t sz);
void* sh_calloc(int id, size_t num, size_t sz);
void* sh_realloc(int id, void *ptr, size_t sz);

void sh_create_extent(void *begin, void *end);

void sh_free(void* ptr);
int get_arena_index(int id);
