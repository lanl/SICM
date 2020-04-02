#include <sys/stat.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <unistd.h>
#include <sys/syscall.h>
#include <errno.h>
#include <sys/types.h>
#include <linux/perf_event.h>
#include <perfmon/pfmlib_perf_event.h>

#define SICM_RUNTIME 1
#include "sicm_runtime.h"
#include "sicm_profilers.h"
#include "sicm_profile.h"

void profile_bw_arena_init(profile_bw_info *);
void profile_bw_deinit();
void profile_bw_init();
void *profile_bw(void *);
void profile_bw_interval(int);
void profile_bw_skip_interval(int);
void profile_bw_post_interval();

/* Uses libpfm to figure out the event we're going to use */
void sh_get_profile_bw_event() {
  int err;
  size_t i, n;
  pfm_perf_encode_arg_t pfm;

  if(!profopts.should_profile_all) {
    /* We don't want to initialize twice, so only initialize if
       we haven't already */
    pfm_initialize();
  }

  /* Make sure all of the events work. Initialize the pes. */
  for(i = 0; i < profopts.num_profile_bw_cpus; i++) {
    for(n = 0; n < profopts.num_profile_bw_events; n++) {
      memset(prof.profile_bw.pes[i][n], 0, sizeof(struct perf_event_attr));
      prof.profile_bw.pes[i][n]->size = sizeof(struct perf_event_attr);
      memset(&pfm, 0, sizeof(pfm_perf_encode_arg_t));
      pfm.size = sizeof(pfm_perf_encode_arg_t);
      pfm.attr = prof.profile_bw.pes[i][n];
  
      err = pfm_get_os_event_encoding(profopts.profile_bw_events[n],
                                      PFM_PLM2 | PFM_PLM3,
                                      PFM_OS_PERF_EVENT,
                                      &pfm);
      if(err != PFM_SUCCESS) {
        fprintf(stderr, "Failed to initialize event '%s'. Aborting.\n", profopts.profile_bw_events[i][n]);
        exit(1);
      }
    }
  }
}

void profile_bw_init() {
  size_t i, n;
  pid_t pid;
  int cpu, group_fd;
  unsigned long flags;
  
  prof.profile_bw.pagesize = (size_t) sysconf(_SC_PAGESIZE);
  
  /* Allocate room for the events profiling info */
  prof.profile->this_interval.profile_bw.events = 
    orig_calloc(profopts.num_profile_bw_events,
                sizeof(per_event_profile_bw_info));
  
  /* Allocate perf structs */
  prof.profile_bw.pes = orig_malloc(sizeof(struct perf_event_attr **) *
                                     profopts.num_profile_bw_cpus);
  prof.profile_bw.fds = orig_malloc(sizeof(int *) *
                                     profopts.num_profile_bw_cpus);
  for(i = 0; i < profopts.num_profile_bw_cpus; i++) {
    prof.profile_bw.pes[i] = orig_malloc(sizeof(struct perf_event_attr *) *
                                          profopts.num_profile_bw_events);
    prof.profile_bw.fds[i] = orig_malloc(sizeof(int) *
                                          profopts.num_profile_bw_events);
    for(n = 0; n < profopts.num_profile_bw_events; n++) {
      prof.profile_bw.pes[i][n] = orig_malloc(sizeof(struct perf_event_attr));
      prof.profile_bw.fds[i][n] = 0;
    }
  }

  /* Use libpfm to fill the pe struct */
  sh_get_profile_bw_event();

  /* Open all perf file descriptors */
  pid = -1;
  cpu = 0;
  group_fd = -1;
  flags = 0;
  for(i = 0; i < profopts.num_profile_bw_cpus; i++) {
    cpu = profopts.profile_bw_cpus[i];
    for(n = 0; n < profopts.num_profile_bw_events; n++) {
      printf("Programming event on CPU %d.\n", cpu);
      prof.profile_bw.fds[i][n] = syscall(__NR_perf_event_open, prof.profile_bw.pes[i][n], pid, cpu, group_fd, flags);
      if(prof.profile_bw.fds[i][n] == -1) {
        fprintf(stderr, "Error opening perf event %d (0x%llx) on cpu %d: %s\n", i, prof.profile_bw.pes[i][n]->config, cpu, strerror(errno));
        exit(1);
      }
    }
  }

  /* Start the events sampling */
  for(i = 0; i < profopts.num_profile_bw_cpus; i++) {
    for(n = 0; n < profopts.num_profile_bw_events; n++) {
      ioctl(prof.profile_bw.fds[i][n], PERF_EVENT_IOC_RESET, 0);
      ioctl(prof.profile_bw.fds[i][n], PERF_EVENT_IOC_ENABLE, 0);
    }
  }
}

void profile_bw_deinit() {
}

void *profile_bw(void *a) {
  pthread_setcanceltype(PTHREAD_CANCEL_ASYNCHRONOUS, NULL);

  while(1) { }
}

void profile_bw_interval(int s) {
  long long count;
  size_t i, n;

  for(i = 0; i < profopts.num_profile_bw_cpus; i++) {
    get_profile_bw_event_prof(i)->current = 0;
    for(n = 0; n < profopts.num_profile_bw_events; n++) {
      ioctl(prof.profile_bw.fds[i][n], PERF_EVENT_IOC_DISABLE, 0);
      read(prof.profile_bw.fds[i][n], &count, sizeof(long long));
      
      /*
      get_profile_bw_event_prof(n)->current += count;
      get_profile_bw_event_prof(n)->total += count;
      */
      
      /* Start it back up again */
      ioctl(prof.profile_bw.fds[i][n], PERF_EVENT_IOC_RESET, 0);
      ioctl(prof.profile_bw.fds[i][n], PERF_EVENT_IOC_ENABLE, 0);
    }
  }
}

void profile_bw_post_interval() {
  per_event_profile_bw_info *per_event_aprof;
  size_t i;

  /* All we need to do here is maintain the peak */
  for(i = 0; i < profopts.num_profile_bw_events; i++) {
    per_event_aprof = get_profile_bw_event_prof(i);
    if(per_event_aprof->current > per_event_aprof->peak) {
      per_event_aprof->peak = per_event_aprof->current;
    }
  }
}

void profile_bw_skip_interval(int s) {
  end_interval();
}
