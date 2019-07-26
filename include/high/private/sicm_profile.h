#pragma once

#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <inttypes.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <sys/types.h>
#include <linux/perf_event.h>
#include <asm/perf_regs.h>
#include <asm/unistd.h>
#include <perfmon/pfmlib_perf_event.h>
#include <time.h>
#include <signal.h>
#include <stdbool.h>
#include <pthread.h>
#include <errno.h>
#include <poll.h>
#include "sicm_high_arenas.h"
#include "sicm_profilers.h"

/* Profiling information for one arena */
typedef struct profile_info {
  size_t first_interval, num_intervals;

  profile_all_info profile_all;
#if 0
  profile_rss_info profile_rss;
  profile_one_info profile_one;
  profile_allocs_info profile_allocs;
#endif
} profile_info;

typedef struct profile_thread {
  pthread_t id;
  int signal;
  unsigned long skip_intervals; /* Number of intervals we should skip */
  unsigned long skipped_intervals; /* Number of intervals we have skipped */
} profile_thread;

typedef struct profiler {
  pthread_t master_id;

  /* One for each profiling thread */
  profile_thread *profile_threads;
  size_t num_profile_threads;

  /* Sync the threads */
  size_t cur_interval, threads_finished;
  pthread_mutex_t mtx;
  pthread_cond_t cond;

  /* Per-arena profiling information */
  profile_info **info;

  /* Data for each profile thread */
  profile_all_data profile_all;
#if 0
  profile_rss_data profile_rss;
  profile_one_data profile_one;
  profile_allocs_data profile_allocs;
#endif
} profiler;

extern profiling_options profopts;
extern tracker_struct tracker;
extern profiler prof;

void sh_start_profile_master_thread();
void sh_stop_profile_master_thread();

void block_signal(int);
void unblock_signal(int);

void *create_profile_arena(int index);
