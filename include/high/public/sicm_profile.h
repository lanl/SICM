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
} arena_profile;

typedef struct interval_profile {
  /* Array of arenas and their info */
  size_t num_arenas;
  arena_profile **arenas;
} interval_profile;

/* Profiling information for a whole application */
typedef struct application_profile {
  size_t num_intervals, num_profile_all_events,
         num_arenas;

  size_t upper_capacity, lower_capacity;

  /* Array of the last interval's arenas */
  arena_profile **arenas;

  /* Array of event strings in the profiling */
  char **profile_all_events;

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
} profiler;

extern profiler prof;

void sh_start_profile_master_thread();
void sh_stop_profile_master_thread();

void end_interval();

void create_arena_profile(int, int);
void add_site_profile(int, int);

static inline void copy_arena_profile(arena_profile *dst, arena_profile *src) {
  memcpy(dst, src, sizeof(arena_profile));
  dst->alloc_sites = orig_malloc(sizeof(int) * dst->num_alloc_sites);
  memcpy(dst->alloc_sites, src->alloc_sites, sizeof(int) * dst->num_alloc_sites);
  dst->profile_all.events = orig_malloc(sizeof(per_event_profile_all_info) * prof.profile->num_profile_all_events);
  memcpy(dst->profile_all.events, src->profile_all.events, sizeof(per_event_profile_all_info) * prof.profile->num_profile_all_events);
}

#define prof_check_good(a, p, i) \
  a = tracker.arenas[i]; \
  p = prof.profile->arenas[i]; \
  if((!a) || (!p)) continue;

#define get_arena_prof(i) \
  prof.profile->arenas[i]

#define get_arena_online_prof(i) \
  (&(get_arena_prof(i)->profile_online))

#define get_arena_all_prof(i) \
  (&(get_arena_prof(i)->profile_all))

/* Since the profiling library stores an interval after it happens,
   the "previous interval" is actually the last one recorded */
#define get_prev_arena_prof(i) \
  prof.cur_interval->arenas[i]

#define get_prev_arena_online_prof(i) \
  (&(get_prev_arena_prof(i)->profile_online))

#define get_arena_profile_all_event_prof(i, n) \
  (&(get_arena_all_prof(i)->events[n]))

