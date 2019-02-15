#pragma once

#include <inttypes.h>
#include "sicm_malloc_free.h"
#include "sicm_low.h"
#include "sicm_tree.h"

#ifdef SICM_RUNTIME
#include "sicm_impl.h"
#else
typedef struct extent_arr extent_arr;
typedef struct sarena sarena;
#endif

extern char sh_initialized;
extern void *(*orig_malloc_ptr)(size_t);
extern void *(*orig_calloc_ptr)(size_t, size_t);
extern void *(*orig_realloc_ptr)(void *, size_t);
extern void (*orig_free_ptr)(void *);

enum arena_layout {
  SHARED_ONE_ARENA, /* One arena between all threads */
  EXCLUSIVE_ONE_ARENA, /* One arena per thread */
  SHARED_DEVICE_ARENAS, /* One arena per device */
  EXCLUSIVE_DEVICE_ARENAS, /* One arena per device per thread */
  SHARED_SITE_ARENAS, /* One arena per allocation site */
  EXCLUSIVE_SITE_ARENAS, /* One arena per allocation site per thread */
  EXCLUSIVE_TWO_DEVICE_ARENAS, /* Two arenas per device per thread */
  EXCLUSIVE_FOUR_DEVICE_ARENAS, /* Four arenas per device per thread */
  BIG_SMALL_ARENAS, /* Per-thread arenas for small allocations, shared site ones for larger sites */
  INVALID_LAYOUT
};

/* Information about a single allocation */
typedef void *addr_t;
typedef struct alloc_info {
  int index;
  size_t size;
} alloc_info;
typedef alloc_info *alloc_info_ptr;
use_tree(addr_t, alloc_info_ptr);

/* Information about a single arena */
typedef struct arena_info {
  int *alloc_sites, num_alloc_sites; /* Stores the allocation sites that are in this arena */
  unsigned index; /* Index into the arenas array */
  sicm_arena arena; /* SICM's low-level interface pointer */
  size_t size; /* The total size of the arena's allocations */
} arena_info;

/* Information about a single site */
typedef sicm_device * deviceptr;
typedef struct site_info {
  pthread_rwlock_t lock;
  deviceptr device;
  int arena;
  size_t size;
  char big;
} site_info;
typedef site_info * siteinfo_ptr;

/* Declare the trees */
use_tree(deviceptr, int);
use_tree(int, siteinfo_ptr);

/* Keeps track of all allocated data: arenas, extents, threads, etc. */
typedef struct tracker_struct {
  /* File to output log to */
  FILE *log_file;

  /* Stores all machine devices and device
   * we should bind to by default */
  struct sicm_device_list device_list;
  int num_numa_nodes;
  deviceptr lower_device, upper_device, default_device;

  /* Stores arena indices associated with a device,
   * for the per-device arena layouts only. */
  pthread_rwlock_t device_arenas_lock;
  tree(deviceptr, int) device_arenas;

  /* Keep track of all extents */
  pthread_rwlock_t extents_lock;
  extent_arr *extents;

  /* Keeps track of sites */
  pthread_rwlock_t sites_lock;
  tree(int, siteinfo_ptr) sites;

  /* Arena layout */
  enum arena_layout layout;
  size_t big_small_threshold;

  /* Keeps track of arenas */
  arena_info **arenas;
  int max_arenas, arenas_per_thread, max_sites_per_arena;
  int max_index;
  pthread_mutex_t arena_lock;

  /* Incremented atomically to keep track of which arena indices are
   * taken or not */
  int arena_counter;

  /* Only for profile_allocs. Stores allocated pointers as keys,
   * their arenas as values. For looking up which arena a `free` call
   * should go to.
   */
  tree(addr_t, alloc_info_ptr) profile_allocs_map;
  pthread_rwlock_t profile_allocs_map_lock;


  /* Associates a thread with an index (starting at 0) into the `arenas` array */
  pthread_mutex_t thread_lock;
  pthread_key_t thread_key;
  int *thread_indices, *orig_thread_indices, *max_thread_indices, max_threads;
  int num_static_sites;

  /* Passes an arena index to the extent hooks */
  int *pending_indices;

  /* Ensures that nothing happens before initialization */
  char finished_initializing;
} tracker_struct;

#define arena_arr_for(i) \
  for(i = 0; i <= tracker.max_index; i++)

#define arena_check_good(a, i) \
  a = tracker.arenas[i]; \
  if(!a) continue;


#define DEFAULT_ARENA_LAYOUT INVALID_LAYOUT

__attribute__((constructor))
void sh_init();

__attribute__((destructor))
void sh_terminate();

void sh_create_extent(sarena *arena, void *begin, void *end);
void sh_delete_extent(sarena *arena, void *begin, void *end);
int get_arena_index(int id, size_t sz);

/* Options for if/when/how to profile. Initialized in src/high/sicm_runtime_init.c,
 * used by src/high/sicm_profile.c.
 */
typedef struct profiling_options {
  /* Should we do profiling? */
  int should_profile_online,
      should_profile_all,
      should_profile_one,
      should_profile_rss,
      should_profile_extent_size,
      should_profile_allocs,
      should_profile,
      should_profile_separate_threads;
  int profile_one_site;
  int should_run_rdspy;
  int profile_intervals;

  /* Sample rates */
  size_t profile_rate_nseconds;
  unsigned long profile_rss_skip_intervals,
                profile_all_skip_intervals,
                profile_extent_size_skip_intervals,
                profile_allocs_skip_intervals,
                profile_online_skip_intervals;
  int sample_freq;
  int max_sample_pages;

  /* Input and output for profiling information */
  FILE *profile_input_file;
  FILE *profile_output_file;
  FILE *profile_online_output_file; /* For the verbose online approach */

  /* Online */
  size_t num_profile_online_events;
  float profile_online_reconf_weight_ratio;
  char **profile_online_events; /* Array of strings of events */
  size_t num_profile_online_weights;
  float *profile_online_weights;
  char profile_online_nobind;
  float profile_online_last_iter_value;
  float profile_online_last_iter_weight;
  unsigned long profile_online_grace_accesses;
  size_t profile_online_hot_intervals;
  int profile_online_use_last_interval;
  int profile_online_debug;
  char profile_online_orig; /* Online strat */
  char profile_online_ski; /* Online strat */

  /* The device to profile bandwidth on */
  deviceptr profile_one_device;

  /* Array of cpu numbers for profile_all */
  size_t num_profile_all_cpus;
  int *profile_all_cpus;

  /* Array of strings for profile_all events */
  size_t num_profile_all_events;
  char **profile_all_events;

  /* Array of strings for profile_one events */
  size_t num_profile_one_events;
  char **profile_one_events;

  /* Array of strings of IMCs for the bandwidth profiling */
  char **imcs;
  int num_imcs, max_imc_len, max_event_len;
} profiling_options;

/* Symbols that both sicm_runtime.c and sicm_profile.c need */
extern tracker_struct tracker;
extern profiling_options profopts;

/* Defined by sicm_profile.c, need a declaration here for sicm_runtime_init.c */
void sh_start_profile_master_thread();
void sh_stop_profile_master_thread();

#ifdef __cplusplus
extern "C" {
#endif
  void* sh_alloc_exact(int id, size_t sz);
  void* sh_alloc(int id, size_t sz);
  void* sh_aligned_alloc(int id, size_t alignment, size_t sz);
  void* sh_memalign(int id, size_t alignment, size_t sz);
  int sh_posix_memalign(int id, void **ptr, size_t alignment, size_t sz);
  void* sh_calloc(int id, size_t num, size_t sz);
  void* sh_realloc(int id, void *ptr, size_t sz);
  void sh_free(void* ptr);
#ifdef __cplusplus
}
#endif
