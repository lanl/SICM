#pragma once
#ifndef _USE_LARGEFILE64
#define _USE_LARGEFILE64
#endif
#include <stdint.h> /* For uint64_t, etc. */
#include <stdlib.h> /* For size_t */
#include "sicm_tree.h"

#include "proc_object_map.h"

/* Going to be defined by sicm_profile.h */
typedef struct arena_profile arena_profile;

/* Going to be defined by sicm_runtime.h */
typedef struct profiling_options profiling_options;
extern profiling_options profopts;

/********************
 * PROFILE_PEBS
 ********************/

struct __attribute__ ((__packed__)) sample {
    uint32_t pid, tid;
    uint64_t addr;
};

union pfn_t {
  uint64_t raw;
  struct {
    uint64_t pfn:        55;
    uint32_t softdirty: 1;
    uint32_t excl:       1;
    uint32_t zero:       4;
    uint32_t filepage:  1;
    uint32_t swapped:    1;
    uint32_t present:    1;
  } obj;
};

typedef struct per_event_profile_pebs_info {
  size_t total, peak, current;
} per_event_profile_pebs_info;

void sh_get_profile_pebs_event();

typedef struct per_arena_profile_pebs_info {
  /* profile_pebs */
  per_event_profile_pebs_info *events;
} per_arena_profile_pebs_info;

typedef struct profile_pebs_info {
  /* Just counts up total accesses that are associated with
     an arena. Overflows eventually. */
  size_t total;
} profile_pebs_info;

typedef struct profile_pebs_data {
  /* For each of these arrays, the first dimension is per-cpu,
   * and the second dimension is per-event. */
  struct perf_event_attr ***pes;
  struct perf_event_mmap_page ***metadata;
  int **fds;
  uint64_t **prev_head;
  size_t pagesize;
  unsigned long tid;
} profile_pebs_data;

/********************
 * PROFILE_BW
 ********************/
typedef struct per_event_profile_bw_info {
  size_t peak, current, total, current_count, total_count;
} per_event_profile_bw_info;

typedef struct per_arena_profile_bw_info {
  /* This is per-arena, but not per-socket. Requires profile_bw_relative. 
     Uses values gathered from profile_pebs. */
  size_t peak, current, total, current_count, total_count;
  /* These track the per-event values in pebs */
  per_event_profile_bw_info *events;
} per_arena_profile_bw_info;
 
typedef struct per_skt_profile_bw_info {
  /* These are in the unit of cache lines per second.
     On most systems, multiply by 64 and divide by 1,000,000 to get MB/s. */
  size_t peak, current, current_count;
} per_skt_profile_bw_info;

typedef struct profile_bw_info {
  per_skt_profile_bw_info *skt;
} profile_bw_info;
 
typedef struct profile_bw_data {
  /* These are one-dimensional arrays that're the size of num_profile_bw_events */
  struct perf_event_attr ****pes;
  int ***fds;
  size_t pagesize;
  struct timespec start, end, actual;
} profile_bw_data;

/********************
 * PROFILE_LATENCY
 ********************/
typedef struct per_skt_profile_latency_info {
  double upper_read_peak, upper_read_current,
         upper_write_peak, upper_write_current,
         lower_read_peak, lower_read_current,
         lower_write_peak, lower_write_current;
  double read_ratio,
         read_ratio_cma,
         write_ratio,
         write_ratio_cma;
} per_skt_profile_latency_info;

typedef struct profile_latency_info {
  per_skt_profile_latency_info *skt;
} profile_latency_info;

typedef struct profile_latency_data {
  /* One per event, per IMC, per socket */
  struct perf_event_attr ****pes;
  int ***fds;
  
  /* One per socket */
  struct perf_event_attr **clocktick_pes;
  int *clocktick_fds;
  
  struct timespec start, end, actual;
  
  /* To keep track of the cumulative moving average */
  double *prev_read_cma, *prev_write_cma;
  size_t num_samples;
} profile_latency_data;

/********************
 * PROFILE_RSS
 ********************/
 
typedef struct profile_rss_info {
  double time;
} profile_rss_info;

typedef struct per_arena_profile_rss_info {
  /* profile_rss */
  size_t peak, current;
  size_t non_present; /* The number of bytes that are allocated to it */
  float present_percentage; /* The percentage of present bytes */
} per_arena_profile_rss_info;

typedef struct profile_rss_data {
  /* profile_rss */
  int pagemap_fd;
  union pfn_t *pfndata;
  size_t pagesize, addrsize;
} profile_rss_data;

/********************
 * PROFILE_OBJECT_MAP
 ********************/
 
unsigned long long get_cgroup_node0_current();
unsigned long long get_cgroup_node1_current();
unsigned long long get_cgroup_node0_max();
size_t get_smaps_rss();
typedef struct profile_objmap_info {
  double time;
  size_t heap_bytes,
         upper_heap_bytes,
         lower_heap_bytes;
  unsigned long long upper_current, lower_current, upper_max, cgroup_memory_current;
} profile_objmap_info;

typedef struct per_arena_profile_objmap_info {
  /* profile_objmap */
  size_t peak_present_bytes, current_present_bytes;
} per_arena_profile_objmap_info;

typedef struct profile_objmap_data {
  /* profile_objmap */
  size_t pagesize;
  pid_t pid;
  struct proc_object_map_t objmap;
  FILE *smaps_file, *node0_current_file, *node1_current_file, *node0_max_file, *memory_current_file, 
       *memory_unaccounted_not_objmap_file;
} profile_objmap_data;

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
  char reconfigure; /* If there was a rebinding this interval */
  char phase_change;
} profile_online_info;

typedef struct per_arena_profile_online_info {
  /* profile_online per-arena */
  char dev; /* The device it was on at the end of the interval.
               0 for lower, 1 for upper, -1 for not yet set. */
  char hot; /* Whether it was hot or not. -1 for not yet set. */
} per_arena_profile_online_info;

typedef struct profile_online_data_ski {
  /* Metrics that only the ski strat needs */
  size_t penalty_move, penalty_stay, penalty_displace,
         total_site_value, site_weight_to_rebind, total_site_weight;
} profile_online_data_ski;

typedef struct profile_online_data {
  
  /* Device lists for the low-level interface */
  sicm_dev_ptr upper_dl, lower_dl;
  
  /* Stats for keeping track of various states */
  char upper_contention, /* Upper tier full? */
       first_upper_contention, /* First interval where upper tier full? */
       first_online_interval; /* First interval where strategy can run? */
  size_t upper_max, upper_used, lower_used;
  
  /* For taking into account an offline profile */
  tree(site_info_ptr, int) offline_sorted_sites;
  size_t offline_invalid_weight;
  
  /* Keep track of the previous hot sites, the current hot sites.
     The bandwidth values correspond to before and after binding these sites. */
  tree(site_info_ptr, int) prev_sorted_sites, cur_sorted_sites;
  size_t prev_bw, cur_bw;
  char prev_interval_reconfigure; /* Indicates if the previous interval reconfigured or not */
  
  FILE *smaps_file;

  /* Strat-specific data */
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

void profile_pebs_init();
void profile_pebs_deinit();
void *profile_pebs(void *);
void profile_pebs_interval(int);
void profile_pebs_post_interval(arena_profile *);
void profile_pebs_skip_interval(int);
void profile_pebs_arena_init(per_arena_profile_pebs_info *);

void *profile_rss(void *);
void profile_rss_interval(int);
void profile_rss_skip_interval(int);
void profile_rss_post_interval(arena_profile *);
void profile_rss_init();
void profile_rss_deinit();
void profile_rss_arena_init(per_arena_profile_rss_info *);

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
void profile_online_arena_init(per_arena_profile_online_info *);

void profile_bw_init();
void profile_bw_deinit();
void *profile_bw(void *);
void profile_bw_interval(int);
void profile_bw_post_interval();
void profile_bw_skip_interval(int);
void profile_bw_arena_init(per_arena_profile_bw_info *);

void profile_latency_init();
void profile_latency_deinit();
void *profile_latency(void *);
void profile_latency_interval(int);
void profile_latency_post_interval();
void profile_latency_skip_interval(int);

void *profile_objmap(void *);
void profile_objmap_interval(int);
void profile_objmap_skip_interval(int);
void profile_objmap_post_interval(arena_profile *);
void profile_objmap_init();
void profile_objmap_deinit();
void profile_objmap_arena_init(per_arena_profile_objmap_info *);
