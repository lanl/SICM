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

void profile_bw_deinit();
void profile_bw_init();
void *profile_bw(void *);
void profile_bw_interval(int);
void profile_bw_skip_interval(int);
void profile_bw_post_interval();

/* Uses libpfm to figure out the event we're going to use */
void sh_get_profile_bw_event() {
  int err;
  size_t i, n, p;
  pfm_perf_encode_arg_t pfm;
  char *tmp;

  if(!should_profile_all()) {
    /* We don't want to initialize twice, so only initialize if
       we haven't already */
    pfm_initialize();
  }

  /* Make sure all of the events work. Initialize the pes. */
  for(i = 0; i < profopts.num_profile_skt_cpus; i++) {
    for(p = 0; p < profopts.num_imcs; p++) {
      for(n = 0; n < profopts.num_profile_bw_events; n++) {
        /* Clear out the data structure that libpfm uses */
        memset(prof.profile_bw.pes[i][p][n], 0, sizeof(struct perf_event_attr));
        prof.profile_bw.pes[i][p][n]->size = sizeof(struct perf_event_attr);
        memset(&pfm, 0, sizeof(pfm_perf_encode_arg_t));
        pfm.size = sizeof(pfm_perf_encode_arg_t);
        pfm.attr = prof.profile_bw.pes[i][p][n];
        
        /* We need to prepend the IMC string to the event name, because libpfm likes that.
           The resulting string will be the IMC name, two colons, then the event name. */
        tmp = malloc(sizeof(char) * (strlen(profopts.profile_bw_events[n]) +
                                     strlen(profopts.imcs[p]) + 3));
        sprintf(tmp, "%s::%s", profopts.imcs[p], profopts.profile_bw_events[n]);
        
        err = pfm_get_os_event_encoding(tmp,
                                        PFM_PLM2 | PFM_PLM3,
                                        PFM_OS_PERF_EVENT,
                                        &pfm);
        if(err != PFM_SUCCESS) {
          fprintf(stderr, "Failed to initialize event '%s'. Aborting.\n", tmp);
          free(tmp);
          exit(1);
        }
        free(tmp);
      }
    }
  }
}

void profile_bw_init() {
  size_t i, n, p;
  pid_t pid;
  int cpu, group_fd;
  unsigned long flags;
  
  prof.profile_bw.pagesize = (size_t) sysconf(_SC_PAGESIZE);
  
  /* Allocate room for the events profiling info */
  prof.profile->this_interval.profile_bw.skt = 
    orig_calloc(profopts.num_profile_skt_cpus,
                sizeof(per_skt_profile_bw_info));
  
  /* Allocate perf structs */
  prof.profile_bw.pes = orig_malloc(sizeof(struct perf_event_attr ***) *
                                     profopts.num_profile_skt_cpus);
  prof.profile_bw.fds = orig_malloc(sizeof(int **) *
                                     profopts.num_profile_skt_cpus);
  for(i = 0; i < profopts.num_profile_skt_cpus; i++) {
    prof.profile_bw.pes[i] = orig_malloc(sizeof(struct perf_event_attr **) *
                                          profopts.num_imcs);
    prof.profile_bw.fds[i] = orig_malloc(sizeof(int *) *
                                          profopts.num_imcs);
    for(p = 0; p < profopts.num_imcs; p++) {
      prof.profile_bw.pes[i][p] = orig_malloc(sizeof(struct perf_event_attr *) *
                                            profopts.num_profile_bw_events);
      prof.profile_bw.fds[i][p] = orig_malloc(sizeof(int) *
                                            profopts.num_profile_bw_events);
      for(n = 0; n < profopts.num_profile_bw_events; n++) {
        prof.profile_bw.pes[i][p][n] = orig_malloc(sizeof(struct perf_event_attr));
        prof.profile_bw.fds[i][p][n] = 0;
      }
    }
  }

  /* Use libpfm to fill the pe struct */
  sh_get_profile_bw_event();

  /* Open all perf file descriptors */
  pid = -1;
  cpu = 0;
  group_fd = -1;
  flags = 0;
  for(i = 0; i < profopts.num_profile_skt_cpus; i++) {
    cpu = profopts.profile_skt_cpus[i];
    for(p = 0; p < profopts.num_imcs; p++) {
      for(n = 0; n < profopts.num_profile_bw_events; n++) {
        prof.profile_bw.fds[i][p][n] = syscall(__NR_perf_event_open, prof.profile_bw.pes[i][p][n], pid, cpu, group_fd, flags);
        if(prof.profile_bw.fds[i][p][n] == -1) {
          fprintf(stderr, "Error opening perf event %d (0x%llx) on cpu %d: %s\n", i, prof.profile_bw.pes[i][p][n]->config, cpu, strerror(errno));
          exit(1);
        }
      }
    }
  }
  
  /* Start the timer just before starting the profiling */
  clock_gettime(CLOCK_MONOTONIC, &(prof.profile_bw.start));

  /* Start the events sampling */
  for(i = 0; i < profopts.num_profile_skt_cpus; i++) {
    for(p = 0; p < profopts.num_imcs; p++) {
      for(n = 0; n < profopts.num_profile_bw_events; n++) {
        ioctl(prof.profile_bw.fds[i][p][n], PERF_EVENT_IOC_RESET, 0);
        ioctl(prof.profile_bw.fds[i][p][n], PERF_EVENT_IOC_ENABLE, 0);
      }
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
  size_t i, n, p, tmp_bw, tmp_all, tmp_arena_all;
  double time;
  arena_info *arena;
  arena_profile *aprof;
  
  /* Get the time between now and the last interval ending */
  clock_gettime(CLOCK_MONOTONIC, &prof.profile_bw.end);
  timespec_diff(&(prof.profile_bw.start),
                &(prof.profile_bw.end),
                &(prof.profile_bw.actual));
  time = prof.profile_bw.actual.tv_sec +
         (((double) prof.profile_bw.actual.tv_nsec) / 1000000000);

  /* Since the profile_cpus array has exactly one CPU per socket,
     we're really just iterating over the sockets here. We'll sum
     the values from each IMC across the socket. */
  for(i = 0; i < profopts.num_profile_skt_cpus; i++) {
    get_profile_bw_skt_prof(i)->current = 0;
    for(p = 0; p < profopts.num_imcs; p++) {
      for(n = 0; n < profopts.num_profile_bw_events; n++) {
        ioctl(prof.profile_bw.fds[i][p][n], PERF_EVENT_IOC_DISABLE, 0);
        read(prof.profile_bw.fds[i][p][n], &count, sizeof(long long));
        
        /* Here, the counter should be gathering the number of retired cache
          lines that go through the IMC. */
        get_profile_bw_skt_prof(i)->current += (((double) count) / time);
      }
    }
  }
  
  if(profopts.profile_bw_relative) {
    /* Clear current values, and add up all profile_all values this interval */
    tmp_all = 0;
    arena_arr_for(n) {
      prof_check_good(arena, aprof, n);
      for(i = 0; i < prof.profile->num_profile_all_events; i++) {
        tmp_all += aprof->profile_all.events[i].current;
      }
    }
    
    /* Add up this interval's bandwidth in tmp */
    tmp_bw = 0;
    for(i = 0; i < profopts.num_profile_skt_cpus; i++) {
      tmp_bw += get_profile_bw_skt_prof(i)->current;
    }
    
    /* Spread this interval's bandwidth amongst the arenas, based on
       their profile_all values. */
    arena_arr_for(n) {
      prof_check_good(arena, aprof, n);
      tmp_arena_all = 0;
      for(i = 0; i < prof.profile->num_profile_all_events; i++) {
        tmp_arena_all += aprof->profile_all.events[i].current;
      }
      if((tmp_arena_all == 0) || (tmp_all == 0) || (tmp_bw == 0)) {
        aprof->profile_bw.current = 0;
      } else {
        aprof->profile_bw.current = (((double) tmp_arena_all) / tmp_all) * tmp_bw;
      }
    }
    
    arena_arr_for(n) {
      prof_check_good(arena, aprof, n);
      aprof->profile_bw.total += aprof->profile_bw.current;
    }
  }
  
  clock_gettime(CLOCK_MONOTONIC, &(prof.profile_bw.start));
  
  for(i = 0; i < profopts.num_profile_skt_cpus; i++) {
    for(p = 0; p < profopts.num_imcs; p++) {
      for(n = 0; n < profopts.num_profile_bw_events; n++) {
        /* Start it back up again */
        ioctl(prof.profile_bw.fds[i][p][n], PERF_EVENT_IOC_RESET, 0);
        ioctl(prof.profile_bw.fds[i][p][n], PERF_EVENT_IOC_ENABLE, 0);
      }
    }
  }
}

void profile_bw_post_interval() {
  per_skt_profile_bw_info *per_skt_aprof;
  size_t i, n;
  arena_info *arena;
  arena_profile *aprof;

  /* All we need to do here is maintain the peak */
  for(i = 0; i < profopts.num_profile_skt_cpus; i++) {
    per_skt_aprof = get_profile_bw_skt_prof(i);
    if(per_skt_aprof->current > per_skt_aprof->peak) {
      per_skt_aprof->peak = per_skt_aprof->current;
    }
  }
  
  if(profopts.profile_bw_relative) {
    /* We'll have to iterate over all of the arenas and maintain
      their peaks, too. */
    arena_arr_for(n) {
      prof_check_good(arena, aprof, n);
      if(aprof->profile_bw.current > aprof->profile_bw.peak) {
        aprof->profile_bw.peak = aprof->profile_bw.current;
      }
    }
  }
}

void profile_bw_skip_interval(int s) {
}
