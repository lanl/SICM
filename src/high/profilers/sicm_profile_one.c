#include <sys/stat.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <unistd.h>
#include <sys/syscall.h>
#include <errno.h>
#include <sys/types.h>
#include "sicm_runtime.h"
#include "sicm_profilers.h"
#include "sicm_profile.h"

#if 0

void profile_one_init() {

    pid = -1;
    cpu = 0;
    group_fd = -1;
    flags = 0;
}

void *profile_one(void *a) {
  int i;

  /* Start the events sampling */
  for(i = 0; i < profopts.num_events; i++) {
    ioctl(prof.fds[i], PERF_EVENT_IOC_RESET, 0);
    ioctl(prof.fds[i], PERF_EVENT_IOC_ENABLE, 0);
  }
  prof.num_bandwidth_intervals = 0;
  prof.running_avg = 0;
  prof.max_bandwidth = 0;

  while(1) { }
}

void profile_one_interval(int s)
{
  float count_f, total;
  long long count;
  int num, i;
  struct itimerspec it;

  /* Stop the counter and read the value if it has been at least a second */
  total = 0;
  for(i = 0; i < profopts.num_events; i++) {
    ioctl(prof.fds[i], PERF_EVENT_IOC_DISABLE, 0);
    read(prof.fds[i], &count, sizeof(long long));
    count_f = (float) count * 64 / 1024 / 1024;
    total += count_f;

    /* Start it back up again */
    ioctl(prof.fds[i], PERF_EVENT_IOC_RESET, 0);
    ioctl(prof.fds[i], PERF_EVENT_IOC_ENABLE, 0);
  }

  printf("%f MB/s\n", total);
  
  /* Calculate the running average */
  prof.num_bandwidth_intervals++;
  prof.running_avg = ((prof.running_avg * (prof.num_bandwidth_intervals - 1)) + total) / prof.num_bandwidth_intervals;

  if(total > prof.max_bandwidth) {
    prof.max_bandwidth = total;
  }
}

/* Uses libpfm to figure out the event we're going to use */
void sh_get_event() {
  int err;
  size_t i;
  pfm_perf_encode_arg_t pfm;

  pfm_initialize();

  /* Make sure all of the events work. Initialize the pes. */
  for(i = 0; i < profopts.num_profile_all_events; i++) {
    memset(prof.profile_all.pes[i], 0, sizeof(struct perf_event_attr));
    prof.profile_all.pes[i]->size = sizeof(struct perf_event_attr);
    memset(&pfm, 0, sizeof(pfm_perf_encode_arg_t));
    pfm.size = sizeof(pfm_perf_encode_arg_t);
    pfm.attr = prof.profile_all.pes[i];

    err = pfm_get_os_event_encoding(profopts.profile_all_events[i], PFM_PLM2 | PFM_PLM3, PFM_OS_PERF_EVENT, &pfm);
    if(err != PFM_SUCCESS) {
      fprintf(stderr, "Failed to initialize event '%s'. Aborting.\n", profopts.profile_all_events[i]);
      exit(1);
    }

    /* If we're profiling all, set some additional options. */
    if(profopts.should_profile_all) {
      prof.profile_all.pes[i]->sample_type = PERF_SAMPLE_ADDR;
      prof.profile_all.pes[i]->sample_period = profopts.sample_freq;
      prof.profile_all.pes[i]->mmap = 1;
      prof.profile_all.pes[i]->disabled = 1;
      prof.profile_all.pes[i]->exclude_kernel = 1;
      prof.profile_all.pes[i]->exclude_hv = 1;
      prof.profile_all.pes[i]->precise_ip = 2;
      prof.profile_all.pes[i]->task = 1;
      prof.profile_all.pes[i]->sample_period = profopts.sample_freq;
    }
  }
}

#endif
