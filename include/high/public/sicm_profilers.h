#pragma once
#define __USE_LARGEFILE64
#include <stdint.h> /* For uint64_t, etc. */
#include <stdlib.h> /* For size_t */
#include "sicm_tree.h"

/* Going to be defined by sicm_profile.h */
typedef struct arena_profile arena_profile;

/* Going to be defined by sicm_runtime.h */
typedef struct profiling_options profiling_options;
extern profiling_options profopts;

/********************
 * PROFILE_ALL
 ********************/

struct __attribute__ ((__packed__)) sample {
    uint32_t pid, tid;
    uint64_t addr;
};

union pfn_t {
  uint64_t raw;
  struct {
    uint64_t pfn:     55;
    uint32_t pshift:  6;
    uint32_t res:     1;
    uint32_t swapped: 1;
    uint32_t present: 1;
  } obj;
};

typedef struct per_event_profile_all_info {
  size_t total, peak, current;
} per_event_profile_all_info;
void sh_get_event();

typedef struct profile_all_info {
  /* profile_all */
  per_event_profile_all_info *events;
} profile_all_info;

typedef struct profile_all_data {
  /* For each of these arrays, the first dimension is per-cpu,
   * and the second dimension is per-event. */
  struct perf_event_attr ***pes;
  struct perf_event_mmap_page ***metadata;
  int **fds;
  uint64_t **prev_head;
  size_t pagesize;
  unsigned long tid;
} profile_all_data;

/********************
 * PROFILE_RSS
 ********************/

typedef struct profile_rss_info {
  /* profile_rss */
  size_t peak, current;
} profile_rss_info;

typedef struct profile_rss_data {
  /* profile_rss */
  int pagemap_fd;
  union pfn_t *pfndata;
  size_t pagesize, addrsize;
} profile_rss_data;

/********************
 * PROFILE_EXTENT_SIZE
 ********************/

typedef struct profile_extent_size_info {
  /* profile_extent_size */
  size_t peak, current;
} profile_extent_size_info;

typedef struct profile_extent_size_data {
  /* profile_extent_size */
} profile_extent_size_data;

/********************
 * PROFILE_ALLOCS
 ********************/

typedef struct profile_allocs_info {
  /* profile_allocs */
  size_t peak, current;
} profile_allocs_info;

typedef struct profile_allocs_data {
  /* profile_allocs */
} profile_allocs_data;

/********************
 * PROFILE_ONLINE
 ********************/

typedef struct application_profile application_profile;
typedef struct site_profile_info site_profile_info;
typedef site_profile_info * site_info_ptr;
typedef struct sicm_device_list * sicm_dev_ptr;

#ifndef SICM_PACKING
#define SICM_PACKING
use_tree(site_info_ptr, int);
use_tree(int, site_info_ptr);
use_tree(int, sicm_dev_ptr);
use_tree(int, size_t);
#endif

typedef struct profile_online_info {
  /* profile_online */
  char dev; /* The device it was on at the end of the interval.
               0 for lower, 1 for upper, -1 for not yet set. */
  char hot; /* Whether it was hot or not. -1 for not yet set. */
  size_t num_hot_intervals; /* How long it's been hot, as of this interval. */
} profile_online_info;

typedef struct profile_online_data_orig {
  /* Metrics that only the orig strat needs */
  size_t total_site_weight, total_site_value, total_sites,
         site_weight_diff, site_value_diff, num_sites_diff,
         site_weight_to_rebind, site_value_to_rebind, num_sites_to_rebind;
} profile_online_data_orig;

typedef struct profile_online_data_ski {
  /* Metrics that only the ski strat needs */
  size_t penalty_move, penalty_stay, penalty_displace,
         total_site_value, site_weight_to_rebind, total_site_weight;
} profile_online_data_ski;

typedef struct profile_online_data {
  size_t num_reconfigures;
  size_t profile_online_event_index;
  sicm_dev_ptr upper_dl, lower_dl;
  char upper_contention; /* Upper tier full? */
  tree(site_info_ptr, int) offline_sorted_sites;

  /* Strat-specific data */
  profile_online_data_orig *orig;
  profile_online_data_ski *ski;
} profile_online_data;

/********************
 * Functions
 ********************
 * Each thread gets:
 * - init
 * - deinit
 * - main
 * - interval
 * - post_interval
 * - skip_interval
 * - arena_init
 *******************/

void profile_all_init();
void profile_all_deinit();
void *profile_all(void *);
void profile_all_interval(int);
void profile_all_post_interval(arena_profile *);
void profile_all_skip_interval(int);
void profile_all_arena_init(profile_all_info *);

void *profile_rss(void *);
void profile_rss_interval(int);
void profile_rss_skip_interval(int);
void profile_rss_post_interval(arena_profile *);
void profile_rss_init();
void profile_rss_deinit();
void profile_rss_arena_init(profile_rss_info *);

void *profile_extent_size(void *);
void profile_extent_size_interval(int);
void profile_extent_size_skip_interval(int);
void profile_extent_size_post_interval(arena_profile *);
void profile_extent_size_init();
void profile_extent_size_deinit();
void profile_extent_size_arena_init(profile_extent_size_info *);

void profile_allocs_init();
void profile_allocs_deinit();
void *profile_allocs(void *);
void profile_allocs_interval(int);
void profile_allocs_post_interval(arena_profile *);
void profile_allocs_skip_interval(int);
void profile_allocs_arena_init(profile_allocs_info *);

void profile_online_init();
void profile_online_deinit();
void *profile_online(void *);
void profile_online_interval(int);
void profile_online_post_interval(arena_profile *);
void profile_online_skip_interval(int);
void profile_online_arena_init(profile_online_info *);
