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

typedef struct profile_thread {
  pthread_t id;
  pthread_mutex_t mtx;

  /* For perf */
  size_t size, total;
  struct perf_event_attr **pes; /* Array of pe structs, for multiple events */
  struct perf_event_mmap_page *metadata;
  int *fds;
  uint64_t consumed;
  struct pollfd pfd;

  /* For libpfm */
  pfm_perf_encode_arg_t *pfm;

  /* For determining RSS */
  int pagemap_fd;
  union pfn_t *pfndata;
  size_t pagesize, addrsize;

  /* For measuring bandwidth */
  timer_t timerid;
  struct sigevent sev;
  size_t num_intervals;
  float running_avg;
} profile_thread;

void sh_start_profile_thread();
void sh_stop_profile_thread();
void *sh_profile_thread(void *);
