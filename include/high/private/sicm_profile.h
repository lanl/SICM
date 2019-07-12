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

/* Options for if/when/how to profile. Initialized in src/high/sicm_high_init.c,
 * used by src/high/sicm_profile.c.
 */
typedef struct profiling_options {
  /* Should we do profiling? */
  int should_profile_online;
  int should_profile_all;
  int should_profile_one;
  int should_profile_rss;
  int should_run_rdspy;

  /* How quickly to sample accesses/RSS */
  float profile_all_rate;
  float profile_rss_rate;
  int sample_freq;
  int max_sample_pages;

  /* The device to profile bandwidth on */
  struct sicm_device *profile_one_device;

  /* Online profiling device and parameters */
  struct sicm_device *online_device;
  ssize_t online_device_cap;

  /* Array of strings for profile_all events */
  size_t num_profile_all_events;
  char **profile_all_events;

  /* Array of strings for profile_one events */
  size_t num_profile_one_events;
  char **profile_one_events;

  /* Array of strings of IMCs for the bandwidth profiling */
  char **imcs;
  int num_imcs, max_imc_len, max_event_len;

  /* Set depending on which type of profiling we're doing */
  size_t num_events;
  char **events;
}
extern profiling_options profopts;

typedef struct profile_thread {

  pthread_mutex_t mtx;
  pthread_t profile_rss_id;
  pthread_t profile_all_id;
  pthread_t profile_one_id;

  /* For perf */
  size_t size, total;
  struct perf_event_attr **pes; /* Array of pe structs, for multiple events */
  struct perf_event_mmap_page *metadata;
  int *fds;
  uint64_t consumed;
  struct pollfd pfd;
  char oops;

  /* For libpfm */
  pfm_perf_encode_arg_t *pfm;

  /* For determining RSS */
  int pagemap_fd;
  union pfn_t *pfndata;
  size_t pagesize, addrsize;

  /* For measuring bandwidth */
  size_t num_intervals;
  float running_avg;
  float max_bandwidth;
} profile_thread;

void sh_start_profile_thread();
void sh_stop_profile_thread();
void *profile_all(void *);
void *profile_one(void *);
void *profile_rss(void *);
