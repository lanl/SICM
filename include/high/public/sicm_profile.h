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

char timespec_cmp(struct timespec *a, struct timespec *b);
void timespec_diff(struct timespec *start,
                   struct timespec *stop,
                   struct timespec *result);

#include "sicm_runtime.h"
#include "sicm_profilers.h"

/* Profiling information for one arena */
typedef struct arena_profile {
  unsigned index;
  int num_alloc_sites, *alloc_sites;

  profile_all_info profile_all;
  profile_rss_info profile_rss;
  profile_extent_size_info profile_extent_size;
  profile_allocs_info profile_allocs;
  profile_online_info profile_online;
  per_arena_profile_bw_info profile_bw;
} arena_profile;

typedef struct interval_profile {
  /* Array of arenas and their info */
  size_t num_arenas;
  arena_profile **arenas;
  
  /* profile_bw doesn't do any per-arena profiling */
  profile_bw_info profile_bw;
} interval_profile;

/* Profiling information for a whole application */
typedef struct application_profile {
  /* Flags that get set if this profile has these types of
     profiling in it */
  char has_profile_all,
       has_profile_allocs,
       has_profile_extent_size,
       has_profile_rss,
       has_profile_online,
       has_profile_bw;
  
  size_t num_intervals, num_profile_all_events;

  size_t upper_capacity, lower_capacity;

  /* Array of the last interval's arenas */
  interval_profile this_interval;

  /* Array of event strings in the profiling */
  char **profile_all_events;
  
  /* Array of integers that are the NUMA nodes of the sockets
     that we got the bandwidth of */
  size_t num_profile_bw_skts;
  int *profile_bw_skts;

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

  /* Convenience pointers */
  interval_profile *cur_interval, *prev_interval;

  /* Sync the threads */
  pthread_mutex_t mtx;
  pthread_cond_t cond;
  char threads_finished;

  /* For the main application thread to
   * signal the master to stop
   */
  int stop_signal, master_signal;

  /* Profiling information for the currently-running application */
  application_profile *profile;
  pthread_rwlock_t profile_lock;

  /* Data for each profile thread */
  profile_all_data profile_all;
  profile_rss_data profile_rss;
  profile_extent_size_data profile_extent_size;
  profile_allocs_data profile_allocs;
  profile_online_data profile_online;
  profile_bw_data profile_bw;
} profiler;

extern profiler prof;

void sh_start_profile_master_thread();
void sh_stop_profile_master_thread();

void end_interval();

void create_arena_profile(int, int);
void add_site_profile(int, int);

/* Given an arena and index, check to make sure it's not NULL */
#define prof_check_good(a, p, i) \
  a = tracker.arenas[i]; \
  p = prof.profile->this_interval.arenas[i]; \
  if((!a) || (!p)) continue;

static inline void copy_arena_profile(arena_profile *dst, arena_profile *src) {
  memcpy(dst, src, sizeof(arena_profile));
  dst->alloc_sites = orig_malloc(sizeof(int) * dst->num_alloc_sites);
  memcpy(dst->alloc_sites, src->alloc_sites, sizeof(int) * dst->num_alloc_sites);
  dst->profile_all.events = orig_malloc(sizeof(per_event_profile_all_info) * prof.profile->num_profile_all_events);
  memcpy(dst->profile_all.events, src->profile_all.events, sizeof(per_event_profile_all_info) * prof.profile->num_profile_all_events);
}

/* Copies an interval profile from the current one
   (stored in pro.profile->this_interval)
   into the array of intervals
   (prof.profile->intervals). */
static inline void copy_interval_profile(size_t index) {
  arena_profile *aprof;
  arena_info *arena;
  interval_profile *interval, *this_interval;
  size_t size, i;
  
  /* Allocate room for the interval that just finished */
  prof.profile->intervals = orig_realloc(prof.profile->intervals,
                                         (index + 1) * sizeof(interval_profile));
                                         
  /* Convenience pointers. We want to copy the contents of
     `this_interval` into `interval`. */
  interval = &(prof.profile->intervals[index]);
  this_interval = &(prof.profile->this_interval);
                                         
  /* Copy the interval_profile from this_interval to intervals[index] */
  interval->num_arenas =
    this_interval->num_arenas;
  interval->arenas =
    orig_calloc(tracker.max_arenas, sizeof(arena_profile *));
    
  /* Copy profile_bw profiling info, too */
  size = profopts.num_profile_bw_cpus * sizeof(per_skt_profile_bw_info);
  interval->profile_bw.skt = orig_malloc(size);
  memcpy(interval->profile_bw.skt,
         this_interval->profile_bw.skt,
         size);
  
  /* Iterate over all of the arenas in the interval, and copy them too */
  arena_arr_for(i) {
    prof_check_good(arena, aprof, i);
    interval->arenas[i] = orig_malloc(sizeof(arena_profile));
    copy_arena_profile(
           interval->arenas[i],
           aprof);
  }
}

#define get_arena_prof(i) \
  prof.profile->this_interval.arenas[i]
  
#define get_profile_bw_prof() \
  (&(prof.profile->this_interval.profile_bw))
  
#define get_arena_online_prof(i) \
  (&(get_arena_prof(i)->profile_online))

#define get_arena_all_prof(i) \
  (&(get_arena_prof(i)->profile_all))
  
#define get_arena_rss_prof(i) \
  (&(get_arena_prof(i)->profile_rss))

/* Since the profiling library stores an interval after it happens,
   the "previous interval" is actually the last one recorded */
#define get_prev_arena_prof(i) \
  prof.cur_interval->arenas[i]

#define get_prev_arena_online_prof(i) \
  (&(get_prev_arena_prof(i)->profile_online))

#define get_arena_profile_all_event_prof(i, n) \
  (&(get_arena_all_prof(i)->events[n]))
  
#define get_profile_bw_skt_prof(i) \
  (&(get_profile_bw_prof()->skt[i]))
  
#define get_profile_bw_arena_prof(i) \
  (&(get_arena_prof(i)->profile_bw))
