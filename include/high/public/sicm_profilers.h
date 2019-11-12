#pragma once
#define __USE_LARGEFILE64
#include <stdint.h> /* For uint64_t, etc. */
#include <stdlib.h> /* For size_t */

/* Going to be defined by sicm_profile.h */
typedef struct arena_profile arena_profile;

/* Going to be defined by sicm_runtime.h */
typedef struct profiling_options profiling_options;
extern profiling_options profopts;

/********************
 * Utilities
 ********************/
struct __attribute__ ((__packed__)) sample {
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
  size_t total, peak, *intervals;
} per_event_profile_all_info;
void sh_get_event();

/********************
 * Profiling information for each arena
 ********************/
typedef struct profile_all_info {
  /* profile_all */
  per_event_profile_all_info *events;
  size_t tmp_accumulator;
} profile_all_info;
typedef struct profile_rss_info {
  /* profile_rss */
  size_t peak, *intervals, tmp_accumulator;
} profile_rss_info;
typedef struct profile_extent_size_info {
  /* profile_extent_size */
  size_t peak, *intervals, tmp_accumulator;
} profile_extent_size_info;
typedef struct profile_allocs_info {
  /* profile_allocs */
  size_t peak, *intervals, tmp_accumulator;
} profile_allocs_info;
typedef struct profile_online_info {
  /* profile_online */
} profile_online_info;

/********************
 * Data needed to do the profiling
 ********************/
typedef struct profile_all_data {
  /* profile_all */
  struct perf_event_attr **pes; /* Array of pe structs, for multiple events */
  struct perf_event_mmap_page **metadata;
  int *fds;
  size_t pagesize;
} profile_all_data;
typedef struct profile_rss_data {
  /* profile_rss */
  int pagemap_fd;
  union pfn_t *pfndata;
  size_t pagesize, addrsize;
} profile_rss_data;
typedef struct profile_extent_size_data {
  /* profile_extent_size */
} profile_extent_size_data;
typedef struct profile_allocs_data {
  /* profile_allocs */
} profile_allocs_data;

/* profile_online */
typedef struct valweight {
  size_t value, weight;
  double value_per_weight;
} valweight;
typedef valweight * valweightptr;
use_tree(valweightptr, size_t);
use_tree(size_t, deviceptr);
typedef struct profile_online_data {
  size_t profile_online_event_index;
  size_t lower_avail_initial, upper_avail_initial;
  struct sicm_device_list *upper_dl, *lower_dl;
  tree(size_t, deviceptr) prev_hotset, prev_coldset;
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
