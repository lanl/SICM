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
  size_t first_interval, num_intervals;
  unsigned index;
  int num_alloc_sites, *alloc_sites;

  profile_all_info profile_all;
  profile_rss_info profile_rss;
  profile_extent_size_info profile_extent_size;
  profile_allocs_info profile_allocs;
  profile_online_info profile_online;
} arena_profile;

/* Profiling information for a whole application */
typedef struct application_profile {
  /* Array of arenas and their info */
  size_t num_arenas;
  arena_profile **arenas;

  /* Array of event strings in the profiling */
  size_t num_profile_all_events;
  char **profile_all_events;
} application_profile;

/* Information about a single profiling thread. Used by the
 * master profiling thread to keep track of them. */
typedef struct profile_thread {
  pthread_t id;
  int signal, skip_signal;
  unsigned long skip_intervals; /* Number of intervals we should skip */
  unsigned long skipped_intervals; /* Number of intervals we have skipped */
  void (*interval_func)(int), /* Per-interval function */
  void (*skip_interval_func)(int), /* Per-interval skip function */
} profile_thread;

typedef struct profiler {
  /* For the master thread */
  pthread_t master_id;
  timer_t timerid;

  /* One for each profiling thread */
  profile_thread *profile_threads;
  size_t num_profile_threads;

  /* Sync the threads */
  size_t cur_interval;
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

void *create_arena_profile(int, int);
void add_site_profile(int, int);


#define prof_check_good(a, p, i) \
  a = tracker.arenas[i]; \
  p = prof.profile->arenas[i]; \
  if((!a) || (!p) || !(p->num_intervals)) continue;
