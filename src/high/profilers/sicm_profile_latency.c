#include <sys/stat.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <unistd.h>
#include <sys/syscall.h>
#include <errno.h>
#include <sys/types.h>
#include <linux/perf_event.h>
#include <perfmon/pfmlib_perf_event.h>
#include <math.h>

#define SICM_RUNTIME 1
#include "sicm_runtime.h"
#include "sicm_profilers.h"
#include "sicm_profile.h"

/* NOTE: This code *heavily* relies on a specific ordering of
   the SH_PROFILE_LATENCY_EVENTS environment variable. It expects the
   first two to contain the read inserts and
   read occupancy, i.e. UNC_M_RPQ_INSERTS and UNC_M_RPQ_OCCUPANCY.
   Lastly, if you want, you can add UNC_M_WPQ_INSERTS UNC_M_WPQ_OCCUPANCY
   to gather writes.
   A separate variable, SH_PROFILE_LATENCY_CLOCKTICK_EVENT, should have the name
   of the event that you use to get DRAM clockticks. */

void profile_latency_deinit();
void profile_latency_init();
void *profile_latency(void *);
void profile_latency_interval(int);
void profile_latency_skip_interval(int);
void profile_latency_post_interval();

/* Uses libpfm to figure out the event we're going to use */
void sh_get_profile_latency_event() {
  int err;
  size_t i, n, p;
  pfm_perf_encode_arg_t pfm;
  char *tmp;

  if(!should_profile_all()) {
    /* We don't want to initialize twice, so only initialize if
       we haven't already */
    pfm_initialize();
  }
  
  /* First, let's look at the event that measures clockticks */
  for(i = 0; i < profopts.num_profile_skt_cpus; i++) {
    /* Clear out the data structure that libpfm uses */
    memset(prof.profile_latency.clocktick_pes[i], 0, sizeof(struct perf_event_attr));
    prof.profile_latency.clocktick_pes[i]->size = sizeof(struct perf_event_attr);
    memset(&pfm, 0, sizeof(pfm_perf_encode_arg_t));
    pfm.size = sizeof(pfm_perf_encode_arg_t);
    pfm.attr = prof.profile_latency.clocktick_pes[i];
    
    /* We need to prepend the IMC string to the event name, because libpfm likes that.
        The resulting string will be the IMC name, two colons, then the event name. */
    tmp = orig_malloc(sizeof(char) * (strlen(profopts.profile_latency_clocktick_event) +
                                 strlen(profopts.imcs[0]) + 3));
    sprintf(tmp, "%s::%s", profopts.imcs[0], profopts.profile_latency_clocktick_event);
    
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

  /* Make sure all of the events work. Initialize the pes. */
  for(i = 0; i < profopts.num_profile_skt_cpus; i++) {
    for(p = 0; p < profopts.num_imcs; p++) {
      for(n = 0; n < profopts.num_profile_latency_events; n++) {
        /* Clear out the data structure that libpfm uses */
        memset(prof.profile_latency.pes[i][p][n], 0, sizeof(struct perf_event_attr));
        prof.profile_latency.pes[i][p][n]->size = sizeof(struct perf_event_attr);
        memset(&pfm, 0, sizeof(pfm_perf_encode_arg_t));
        pfm.size = sizeof(pfm_perf_encode_arg_t);
        pfm.attr = prof.profile_latency.pes[i][p][n];
        
        /* We need to prepend the IMC string to the event name, because libpfm likes that.
           The resulting string will be the IMC name, two colons, then the event name. */
        tmp = orig_malloc(sizeof(char) * (strlen(profopts.profile_latency_events[n]) +
                                     strlen(profopts.imcs[p]) + 3));
        sprintf(tmp, "%s::%s", profopts.imcs[p], profopts.profile_latency_events[n]);
        
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

void profile_latency_init() {
  size_t i, n, p;
  pid_t pid;
  int cpu, group_fd;
  unsigned long flags;
  
  /* Allocate room for the events profiling info */
  get_profile_latency_prof()->skt = 
    orig_calloc(profopts.num_profile_skt_cpus,
                sizeof(per_skt_profile_latency_info));
  
  /* Allocate perf structs */
  prof.profile_latency.pes = orig_malloc(sizeof(struct perf_event_attr ***) *
                                     profopts.num_profile_skt_cpus);
  prof.profile_latency.fds = orig_malloc(sizeof(int **) *
                                     profopts.num_profile_skt_cpus);
  prof.profile_latency.clocktick_pes = orig_malloc(sizeof(struct perf_event_attr *) *
                                     profopts.num_profile_skt_cpus);
  prof.profile_latency.clocktick_fds = orig_malloc(sizeof(int) *
                                     profopts.num_profile_skt_cpus);
  for(i = 0; i < profopts.num_profile_skt_cpus; i++) {
    prof.profile_latency.clocktick_pes[i] = orig_malloc(sizeof(struct perf_event_attr));
    prof.profile_latency.clocktick_fds[i] = 0;
    prof.profile_latency.pes[i] = orig_malloc(sizeof(struct perf_event_attr **) *
                                          profopts.num_imcs);
    prof.profile_latency.fds[i] = orig_malloc(sizeof(int *) *
                                          profopts.num_imcs);
    for(p = 0; p < profopts.num_imcs; p++) {
      prof.profile_latency.pes[i][p] = orig_malloc(sizeof(struct perf_event_attr *) *
                                            profopts.num_profile_latency_events);
      prof.profile_latency.fds[i][p] = orig_malloc(sizeof(int) *
                                            profopts.num_profile_latency_events);
      for(n = 0; n < profopts.num_profile_latency_events; n++) {
        prof.profile_latency.pes[i][p][n] = orig_malloc(sizeof(struct perf_event_attr));
        prof.profile_latency.fds[i][p][n] = 0;
      }
    }
  }
  
  /* Use libpfm to fill the pe structs */
  sh_get_profile_latency_event();

  /* Open all perf file descriptors */
  pid = -1;
  cpu = 0;
  group_fd = -1;
  flags = 0;
  for(i = 0; i < profopts.num_profile_skt_cpus; i++) {
    cpu = profopts.profile_skt_cpus[i];
    prof.profile_latency.clocktick_fds[i] = syscall(__NR_perf_event_open, prof.profile_latency.clocktick_pes[i], pid, cpu, group_fd, flags);
    if(prof.profile_latency.clocktick_fds[i] == -1) {
      fprintf(stderr, "Error opening perf event %d (0x%llx) on cpu %d: %s\n", i, prof.profile_latency.clocktick_pes[i]->config, cpu, strerror(errno));
      exit(1);
    }
    for(p = 0; p < profopts.num_imcs; p++) {
      for(n = 0; n < profopts.num_profile_latency_events; n++) {
        prof.profile_latency.fds[i][p][n] = syscall(__NR_perf_event_open, prof.profile_latency.pes[i][p][n], pid, cpu, group_fd, flags);
        if(prof.profile_latency.fds[i][p][n] == -1) {
          fprintf(stderr, "Error opening perf event %d (0x%llx) on cpu %d: %s\n", i, prof.profile_latency.pes[i][p][n]->config, cpu, strerror(errno));
          exit(1);
        }
      }
    }
  }
  
  prof.profile_latency.prev_read_cma = calloc(sizeof(double), profopts.num_profile_skt_cpus);
  prof.profile_latency.prev_write_cma = calloc(sizeof(double), profopts.num_profile_skt_cpus);
  prof.profile_latency.num_samples = 0;
  
  /* Start the timer just before starting the profiling */
  clock_gettime(CLOCK_MONOTONIC, &(prof.profile_latency.start));

  /* Start the events sampling */
  for(i = 0; i < profopts.num_profile_skt_cpus; i++) {
    ioctl(prof.profile_latency.clocktick_fds[i], PERF_EVENT_IOC_RESET, 0);
    ioctl(prof.profile_latency.clocktick_fds[i], PERF_EVENT_IOC_ENABLE, 0);
    for(p = 0; p < profopts.num_imcs; p++) {
      for(n = 0; n < profopts.num_profile_latency_events; n++) {
        ioctl(prof.profile_latency.fds[i][p][n], PERF_EVENT_IOC_RESET, 0);
        ioctl(prof.profile_latency.fds[i][p][n], PERF_EVENT_IOC_ENABLE, 0);
      }
    }
  }
}

void profile_latency_deinit() {
}

void *profile_latency(void *a) {
  pthread_setcanceltype(PTHREAD_CANCEL_ASYNCHRONOUS, NULL);

  while(1) { }
}

void profile_latency_interval(int s) {
  long long counter;
  size_t i, n, p;
  double time, dram_speed,
         upper_read_occupancy,
         upper_read_inserts,
         upper_write_occupancy,
         upper_write_inserts,
         lower_read_occupancy,
         lower_read_inserts,
         lower_write_occupancy,
         lower_write_inserts,
         geomean;
  
  /* Get the time between now and the last interval ending */
  clock_gettime(CLOCK_MONOTONIC, &(prof.profile_latency.end));
  timespec_diff(&(prof.profile_latency.start),
                &(prof.profile_latency.end),
                &(prof.profile_latency.actual));
  time = prof.profile_latency.actual.tv_sec +
         (((double) prof.profile_latency.actual.tv_nsec) / 1000000000);
  
  /* Freeze all the counters, and zero out the current values */       
  for(i = 0; i < profopts.num_profile_skt_cpus; i++) {
    ioctl(prof.profile_latency.clocktick_fds[i], PERF_EVENT_IOC_DISABLE, 0);
    get_profile_latency_skt_prof(i)->upper_read_current = 0;
    get_profile_latency_skt_prof(i)->upper_write_current = 0;
    get_profile_latency_skt_prof(i)->lower_read_current = 0;
    get_profile_latency_skt_prof(i)->lower_write_current = 0;
    get_profile_latency_skt_prof(i)->read_ratio = 0;
    get_profile_latency_skt_prof(i)->write_ratio = 0;
    for(p = 0; p < profopts.num_imcs; p++) {
      for(n = 0; n < profopts.num_profile_latency_events; n++) {
        ioctl(prof.profile_latency.fds[i][p][n], PERF_EVENT_IOC_DISABLE, 0);
      }
    }
  }

  /* First, grab the speed of the DRAM during that interval.
     Also add up the occupancy and inserts. */
  for(i = 0; i < profopts.num_profile_skt_cpus; i++) {
    /* Update the dramclocks, get the speed of DRAM in nanoseconds */
    read(prof.profile_latency.clocktick_fds[i], &counter, sizeof(long long));
    dram_speed = (counter) / (1e9 * time);
    
    upper_read_inserts = 0;
    upper_read_occupancy = 0;
    upper_write_inserts = 0;
    upper_write_occupancy = 0;
    lower_read_inserts = 0;
    lower_read_occupancy = 0;
    lower_write_inserts = 0;
    lower_write_occupancy = 0;
    for(p = 0; p < profopts.num_imcs; p++) {
      /* Upper tier read and write */
      read(prof.profile_latency.fds[i][p][0], &counter, sizeof(long long));
      upper_read_inserts += ((double) counter);
      read(prof.profile_latency.fds[i][p][1], &counter, sizeof(long long));
      upper_read_occupancy += ((double) counter);
      read(prof.profile_latency.fds[i][p][2], &counter, sizeof(long long));
      upper_write_inserts += ((double) counter);
      read(prof.profile_latency.fds[i][p][3], &counter, sizeof(long long));
      upper_write_occupancy += ((double) counter);
        
      /* Lower tier read and write */
      read(prof.profile_latency.fds[i][p][4], &counter, sizeof(long long));
      lower_read_inserts += ((double) counter);
      read(prof.profile_latency.fds[i][p][5], &counter, sizeof(long long));
      lower_read_occupancy += ((double) counter);
      read(prof.profile_latency.fds[i][p][6], &counter, sizeof(long long));
      lower_write_inserts += ((double) counter);
      read(prof.profile_latency.fds[i][p][7], &counter, sizeof(long long));
      lower_write_occupancy += ((double) counter);
    }
    
    /* Now we can calculate our one value for this socket. */
    if(upper_read_occupancy && upper_read_inserts) {
      get_profile_latency_skt_prof(i)->upper_read_current = (upper_read_occupancy / upper_read_inserts) / dram_speed;
    }
    if(upper_write_occupancy && upper_write_inserts) {
      get_profile_latency_skt_prof(i)->upper_write_current = (upper_write_occupancy / upper_write_inserts) / dram_speed;
    }
    if(lower_read_occupancy && lower_read_inserts) {
      get_profile_latency_skt_prof(i)->lower_read_current = (lower_read_occupancy / lower_read_inserts) / dram_speed;
    }
    if(lower_write_occupancy && lower_write_inserts) {
      get_profile_latency_skt_prof(i)->lower_write_current = (lower_write_occupancy / lower_write_inserts) / dram_speed;
    }
    if(get_profile_latency_skt_prof(i)->upper_read_current &&
       get_profile_latency_skt_prof(i)->lower_read_current) {
      get_profile_latency_skt_prof(i)->read_ratio = (get_profile_latency_skt_prof(i)->lower_read_current /
                                                     get_profile_latency_skt_prof(i)->upper_read_current);
      get_profile_latency_skt_prof(i)->read_ratio_cma = ((prof.profile_latency.prev_read_cma[i] * prof.profile_latency.num_samples) +
                                                         get_profile_latency_skt_prof(i)->read_ratio) /
                                                        (prof.profile_latency.num_samples + 1);
    } else {
      get_profile_latency_skt_prof(i)->read_ratio_cma = prof.profile_latency.prev_read_cma[i];
    }
    prof.profile_latency.prev_read_cma[i] = get_profile_latency_skt_prof(i)->read_ratio_cma;
    if(get_profile_latency_skt_prof(i)->upper_write_current &&
       get_profile_latency_skt_prof(i)->lower_write_current) {
      get_profile_latency_skt_prof(i)->write_ratio = (get_profile_latency_skt_prof(i)->lower_write_current /
                                                     get_profile_latency_skt_prof(i)->upper_write_current);
      get_profile_latency_skt_prof(i)->write_ratio_cma = ((prof.profile_latency.prev_write_cma[i] * prof.profile_latency.num_samples) +
                                                         get_profile_latency_skt_prof(i)->write_ratio) /
                                                        (prof.profile_latency.num_samples + 1);
    } else {
      get_profile_latency_skt_prof(i)->write_ratio_cma = prof.profile_latency.prev_write_cma[i];
    }
    prof.profile_latency.prev_write_cma[i] = get_profile_latency_skt_prof(i)->write_ratio_cma;
  }
  
  if(profopts.profile_latency_set_multipliers) {
    /* We'll take the geometric mean of all sockets' latency ratios,
      and use that to modify the multipliers to weight PROFILE_ALL
      accesses. This feature depends on there being at least two
      PROFILE_ALL events, and the first even being for the upper tier,
      and the second being for the lower tier. */
    geomean = 0.0;
    for(i = 0; i < profopts.num_profile_skt_cpus; i++) {
      geomean += log(get_profile_latency_skt_prof(i)->read_ratio_cma);
    }
    geomean /= profopts.num_profile_skt_cpus;
    geomean = exp(geomean);
    if(profopts.num_profile_all_multipliers >= 2) {
      profopts.profile_all_multipliers[0] = 1;
      if(profopts.profile_all_multipliers[1] > 1) {
        profopts.profile_all_multipliers[1] = geomean;
      } else {
        profopts.profile_all_multipliers[1] = 1;
      }
    }
  }
  
  prof.profile_latency.num_samples++;
  clock_gettime(CLOCK_MONOTONIC, &(prof.profile_latency.start));
  
  for(i = 0; i < profopts.num_profile_skt_cpus; i++) {
    ioctl(prof.profile_latency.clocktick_fds[i], PERF_EVENT_IOC_RESET, 0);
    ioctl(prof.profile_latency.clocktick_fds[i], PERF_EVENT_IOC_ENABLE, 0);
    for(p = 0; p < profopts.num_imcs; p++) {
      for(n = 0; n < profopts.num_profile_latency_events; n++) {
        /* Start it back up again */
        ioctl(prof.profile_latency.fds[i][p][n], PERF_EVENT_IOC_RESET, 0);
        ioctl(prof.profile_latency.fds[i][p][n], PERF_EVENT_IOC_ENABLE, 0);
      }
    }
  }
}

void profile_latency_post_interval() {
  per_skt_profile_latency_info *per_skt_aprof;
  size_t i, n;

  /* All we need to do here is maintain the peak */
  for(i = 0; i < profopts.num_profile_skt_cpus; i++) {
    per_skt_aprof = get_profile_latency_skt_prof(i);
    if(per_skt_aprof->upper_read_current > per_skt_aprof->upper_read_peak) {
      per_skt_aprof->upper_read_peak = per_skt_aprof->upper_read_current;
    }
    if(per_skt_aprof->upper_write_current > per_skt_aprof->upper_write_peak) {
      per_skt_aprof->upper_write_peak = per_skt_aprof->upper_write_current;
    }
    if(per_skt_aprof->lower_read_current > per_skt_aprof->lower_read_peak) {
      per_skt_aprof->lower_read_peak = per_skt_aprof->lower_read_current;
    }
    if(per_skt_aprof->lower_write_current > per_skt_aprof->lower_write_peak) {
      per_skt_aprof->lower_write_peak = per_skt_aprof->lower_write_current;
    }
  }
}

void profile_latency_skip_interval(int s) {
}
