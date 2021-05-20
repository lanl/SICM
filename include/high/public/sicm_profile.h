#pragma once

#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <inttypes.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <sys/types.h>
#include <asm/perf_regs.h>
#include <asm/unistd.h>
#include <time.h>
#include <signal.h>
#include <stdbool.h>
#include <pthread.h>
#include <errno.h>
#include <poll.h>

/* Returns 0 if "a" is bigger, 1 if "b" is bigger */
static char timespec_cmp(struct timespec *a, struct timespec *b) {
  if (a->tv_sec == b->tv_sec) {
    if(a->tv_nsec > b->tv_nsec) {
      return 0;
    } else {
      return 1;
    }
  } else if(a->tv_sec > b->tv_sec) {
    return 0;
  } else {
    return 1;
  }
}

/* Subtracts two timespec structs from each other. Assumes stop is
 * larger than start.
 */
static void timespec_diff(struct timespec *start, struct timespec *stop,
                   struct timespec *result) {
  if ((stop->tv_nsec - start->tv_nsec) < 0) {
    result->tv_sec = stop->tv_sec - start->tv_sec - 1;
    result->tv_nsec = stop->tv_nsec - start->tv_nsec + 1000000000;
  } else {
    result->tv_sec = stop->tv_sec - start->tv_sec;
    result->tv_nsec = stop->tv_nsec - start->tv_nsec;
  }
}

#include "sicm_runtime.h"
#include "sicm_profilers.h"

/* Profiling information for one arena */
typedef struct arena_profile {
  unsigned index;
  int num_alloc_sites, *alloc_sites;
  
  /* Boolean that indicates that this arena's profiling information is invalid.
     One example of this is the per-thread arenas in some arena layouts, which
     aggregate multiple sites' allocations. */
  char invalid;

  per_arena_profile_pebs_info profile_pebs;
  profile_extent_size_info profile_extent_size;
  profile_allocs_info profile_allocs;
  per_arena_profile_rss_info profile_rss;
  per_arena_profile_online_info profile_online;
  per_arena_profile_bw_info profile_bw;
  per_arena_profile_objmap_info profile_objmap;
} arena_profile;

typedef struct interval_profile {
  /* Time in seconds that this interval took */
  double time;
  
  /* Array of arenas and their info */
  size_t num_arenas, max_index;
  arena_profile **arenas;
  
  /* These are profiling types that can have not-per-arena
     profiling information */
  profile_latency_info profile_latency;
  profile_bw_info profile_bw;
  profile_online_info profile_online;
  profile_rss_info profile_rss;
  profile_pebs_info profile_pebs;
  profile_objmap_info profile_objmap;
} interval_profile;

/* Profiling information for a whole application */
typedef struct application_profile {
  /* Flags that get set if this profile has these types of
     profiling in it */
  char has_profile_pebs,
       has_profile_allocs,
       has_profile_extent_size,
       has_profile_rss,
       has_profile_online,
       has_profile_bw,
       has_profile_bw_relative,
       has_profile_latency,
       has_profile_objmap;
  
  size_t num_intervals;

  /* Array of PROFILE_PEBS event strings */
  size_t num_profile_pebs_events;
  char **profile_pebs_events;
  
  /* Array of NUMA IDs for PROFILE_BW */
  size_t num_profile_skts;
  int *profile_skts;

  interval_profile *intervals;
} application_profile;

/* Information about a single profiling thread. Used by the
 * master profiling thread to keep track of them. */
typedef struct profile_thread {
  pthread_t id;
  int signal, skip_signal;
  unsigned long skip_intervals; /* Number of intervals we should skip */
  unsigned long skipped_intervals; /* Number of intervals we have skipped */
  void (*interval_func)(int); /* Per-interval function */
  void (*skip_interval_func)(int); /* Per-interval skip function */
} profile_thread;

typedef struct profiler {
  /* For the master thread */
  pthread_t master_id;
  timer_t timerid;

  /* One for each profiling thread */
  profile_thread *profile_threads;
  size_t num_profile_threads;

  /* Sync the threads */
  pthread_mutex_t mtx;
  pthread_cond_t cond;

  /* For the main application thread to
   * signal the master to stop
   */
  int stop_signal, master_signal;
  struct timespec start, end;
  double target;

  /* Profiling so far */
  application_profile *profile;
  
  interval_profile cur_interval, prev_interval;

  /* Data for each profile thread */
  profile_pebs_data profile_pebs;
  profile_rss_data profile_rss;
  profile_extent_size_data profile_extent_size;
  profile_allocs_data profile_allocs;
  profile_online_data profile_online;
  profile_bw_data profile_bw;
  profile_latency_data profile_latency;
  profile_objmap_data profile_objmap;
} profiler;

extern profiler prof;

void sh_start_profile_master_thread();
void sh_stop_profile_master_thread();
void create_arena_profile(int, int, char);
void create_extent_objmap_entry(void*, void*);
void delete_extent_objmap_entry(void*);
void add_site_profile(int, int);

/* Given an arena and index, check to make sure it's not NULL */
#define prof_check_good(a, p, i) \
  a = tracker.arenas[i]; \
  p = prof.cur_interval.arenas[i]; \
  if((!a) || (!p)) continue;
  
#define aprof_arr_for(i, aprof) \
  for(i = 0; i <= prof.cur_interval.max_index; i++)
    
#define aprof_check_good(i, aprof) \
    aprof = prof.cur_interval.arenas[i]; \
    if(!aprof) continue;

static inline void copy_arena_profile(arena_profile *dst, arena_profile *src) {
  memcpy(dst, src, sizeof(arena_profile));
  if(!(dst->alloc_sites)) {
    dst->alloc_sites = internal_malloc(sizeof(int) * tracker.max_sites_per_arena);
  }
  memcpy(dst->alloc_sites, src->alloc_sites, sizeof(int) * tracker.max_sites_per_arena);
  if(!(dst->profile_pebs.events)) {
    dst->profile_pebs.events = internal_malloc(sizeof(per_event_profile_pebs_info) * prof.profile->num_profile_pebs_events);
  }
  memcpy(dst->profile_pebs.events, src->profile_pebs.events, sizeof(per_event_profile_pebs_info) * prof.profile->num_profile_pebs_events);
  if(!(dst->profile_bw.events)) {
    dst->profile_bw.events = internal_malloc(sizeof(per_event_profile_bw_info) * prof.profile->num_profile_pebs_events);
  }
  memcpy(dst->profile_bw.events, src->profile_bw.events, sizeof(per_event_profile_bw_info) * prof.profile->num_profile_pebs_events);
}

static inline void copy_interval_profile(interval_profile *dest, interval_profile *src) {
  arena_profile *aprof;
  size_t size, i;
  
  if(!src || !dest) {
    return;
  }
  
  memcpy(dest, src, sizeof(interval_profile));
  
  if(!(dest->arenas)) {
    dest->arenas = internal_calloc(tracker.max_arenas, sizeof(arena_profile *));
  }
    
  dest->profile_bw.skt = NULL;
  if(should_profile_bw()) {
    size = profopts.num_profile_skt_cpus * sizeof(per_skt_profile_bw_info);
    if(!(dest->profile_bw.skt)) {
      dest->profile_bw.skt = internal_malloc(size);
    }
    memcpy(dest->profile_bw.skt,
          src->profile_bw.skt,
          size);
  }
  
  dest->profile_latency.skt = NULL;
  if(should_profile_latency()) {
    size = profopts.num_profile_skt_cpus * sizeof(per_skt_profile_latency_info);
    if(!(dest->profile_latency.skt)) {
      dest->profile_latency.skt = internal_malloc(size);
    }
    memcpy(dest->profile_latency.skt,
          src->profile_latency.skt,
          size);
  }
  
  aprof_arr_for(i, aprof) {
    aprof_check_good(i, aprof);
    aprof = src->arenas[i];
    if(!aprof) continue;
    
    if(!(dest->arenas[i])) {
      dest->arenas[i] = internal_malloc(sizeof(arena_profile));
    }
    copy_arena_profile(dest->arenas[i], aprof);
  }
}

/* These are all convenience "functions" for accessing
   parts of the profile. */
#define get_arena_prof(i) \
  prof.cur_interval.arenas[i]
#define get_prev_arena_prof(i) \
  prof.prev_interval.arenas[i]
  
/* profile_pebs */
#define get_pebs_prof() \
  (&(prof.cur_interval.profile_pebs))
#define get_pebs_arena_prof(i) \
  (&(get_arena_prof(i)->profile_pebs))
#define get_pebs_event_prof(i, n) \
  (&(get_pebs_arena_prof(i)->events[n]))
  
/* profile_allocs */
#define get_allocs_prof() \
  (&(prof.cur_interval.profile_allocs))
#define get_allocs_arena_prof(i) \
  (&(get_arena_prof(i)->profile_allocs))
  
/* profile_bw */
#define get_bw_prof() \
  (&(prof.cur_interval.profile_bw))
#define get_bw_arena_prof(i) \
  (&(get_arena_prof(i)->profile_bw))
#define get_bw_event_prof(i, n) \
  (&(get_bw_arena_prof(i)->events[n]))
#define get_bw_skt_prof(i) \
  (&(get_bw_prof()->skt[i]))
  
/* profile_rss */
#define get_rss_prof() \
  (&(prof.cur_interval.profile_rss))
#define get_rss_arena_prof(i) \
  (&(get_arena_prof(i)->profile_rss))
  
/* profile_online */
#define get_online_prof() \
  (&(prof.cur_interval.profile_online))
#define get_online_arena_prof(i) \
  (&(get_arena_prof(i)->profile_online))
#define get_online_prev_arena_prof(i) \
  (&(get_prev_arena_prof(i)->profile_online))
#define get_online_data() \
  (&(prof.profile_online))
  
/* profile_obmap */
#define get_objmap_prof() \
  (&(prof.cur_interval.profile_objmap))
#define get_objmap_arena_prof(i) \
  (&(get_arena_prof(i)->profile_objmap))
  
/* profile_latency */
#define get_latency_prof() \
  (&(prof.cur_interval.profile_latency))
#define get_latency_skt_prof(i) \
  (&(get_latency_prof()->skt[i]))
