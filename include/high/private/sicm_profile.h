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
#include "sicm_high.h"

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

typedef struct profile_thread {
  pthread_t id; /* e.g. returned by pthread_self */
  pid_t *tid; /* e.g. returned by gettid. */
  timer_t timer;
  struct sigevent sev;
  struct sigaction sa;
  struct itimerspec its;
  void (*func)(int);
} profile_thread;

typedef struct profiler {

  pthread_mutex_t mtx;
  profile_thread profile_all;
  profile_thread profile_one;
  profile_thread profile_rss;
  profile_thread profile_allocs;

  /* Keeping track of intervals */
  size_t cur_interval;

  /* For perf */
  size_t size;
  struct perf_event_attr **pes; /* Array of pe structs, for multiple events */
  struct perf_event_mmap_page **metadata;
  int *fds;
  struct pollfd pfd;

  /* For libpfm */
  pfm_perf_encode_arg_t *pfm;

  /* For determining RSS */
  int pagemap_fd;
  union pfn_t *pfndata;
  size_t pagesize, addrsize;

  /* For measuring bandwidth */
  size_t num_bandwidth_intervals;
  float running_avg;
  float max_bandwidth;
} profiler;

extern profiling_options profopts;
extern tracker_struct tracker;
extern profiler prof;

void sh_start_profile_thread();
void sh_stop_profile_thread();
void *profile_all(void *);
void *profile_one(void *);
void *profile_rss(void *);
void *profile_allocs(void *);
