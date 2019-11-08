#include <sys/stat.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <unistd.h>
#include <sys/syscall.h>
#include <errno.h>
#include <sys/types.h>

#define SICM_RUNTIME 1
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
#endif
