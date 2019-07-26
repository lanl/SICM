#pragma once
#define __USE_LARGEFILE64
#include "sicm_profile.h"

/* Each type of profiling gets an initialization and deinitialization
 * function, both of which get run in the main SICM thread. Each also
 * gets a spinning loop function, a per-interval function, and a struct
 * to store data.
 */

/* ALL */
struct __attribute__ ((__packed__)) sample {
    uint64_t addr;
};
typedef struct profile_all_data {
  /* For perf */
  size_t size;
  struct perf_event_attr **pes; /* Array of pe structs, for multiple events */
  struct perf_event_mmap_page **metadata;
  int *fds;
  struct pollfd pfd;

  /* For libpfm */
  pfm_perf_encode_arg_t *pfm;
} profile_all_data;
void sh_get_event();
void profile_all_init();
void profile_all_deinit();
void *profile_all(void *a);
void profile_all_interval(int s);

#if 0
/* RSS */
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
typedef struct profile_rss_data {
  int pagemap_fd;
  union pfn_t *pfndata;
  size_t pagesize, addrsize;
} profile_rss_data;
void *profile_rss(void *a);
void profile_rss_interval(int s);

/* ONE */
typedef struct profile_one_data {
  /* For measuring bandwidth */
  size_t num_bandwidth_intervals;
  float running_avg;
  float max_bandwidth;
} profile_one_data;
void *profile_one(void *a);
void profile_one_interval(int s);

/* ALLOCS */
typedef struct profile_allocs_data {
} profile_allocs_data;
void *profile_allocs(void *a);
void profile_allocs_interval(int s);
#endif
