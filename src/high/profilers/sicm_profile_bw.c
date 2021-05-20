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
void profile_bw_arena_init();
void *profile_bw(void *);
void profile_bw_interval(int);
void profile_bw_skip_interval(int);
void profile_bw_post_interval();

void profile_bw_arena_init(per_arena_profile_bw_info *info) {
  size_t i;

  info->events = internal_calloc(prof.profile->num_profile_pebs_events, sizeof(per_event_profile_bw_info));
  for(i = 0; i < prof.profile->num_profile_pebs_events; i++) {
    info->events[i].total = 0;
    info->events[i].peak = 0;
    info->events[i].current = 0;
    info->events[i].total_count = 0;
  }
}

/* Uses libpfm to figure out the event we're going to use */
void sh_get_bw_event() {
  int err;
  size_t i, n, p;
  pfm_perf_encode_arg_t pfm;
  char *tmp;

  if(!should_profile_pebs()) {
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
        tmp = internal_malloc(sizeof(char) * (strlen(profopts.profile_bw_events[n]) +
                                     strlen(profopts.imcs[p]) + 3));
        sprintf(tmp, "%s::%s", profopts.imcs[p], profopts.profile_bw_events[n]);
        
        err = pfm_get_os_event_encoding(tmp,
                                        PFM_PLM2 | PFM_PLM3,
                                        PFM_OS_PERF_EVENT,
                                        &pfm);
        if(err != PFM_SUCCESS) {
          fprintf(stderr, "Failed to initialize bw event '%s'. Aborting.\n", tmp);
          internal_free(tmp);
          exit(1);
        }
        internal_free(tmp);
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
  get_bw_prof()->skt = 
    internal_calloc(profopts.num_profile_skt_cpus,
                sizeof(per_skt_profile_bw_info));
  
  /* Allocate perf structs */
  prof.profile_bw.pes = internal_malloc(sizeof(struct perf_event_attr ***) *
                                     profopts.num_profile_skt_cpus);
  prof.profile_bw.fds = internal_malloc(sizeof(int **) *
                                     profopts.num_profile_skt_cpus);
  for(i = 0; i < profopts.num_profile_skt_cpus; i++) {
    prof.profile_bw.pes[i] = internal_malloc(sizeof(struct perf_event_attr **) *
                                          profopts.num_imcs);
    prof.profile_bw.fds[i] = internal_malloc(sizeof(int *) *
                                          profopts.num_imcs);
    for(p = 0; p < profopts.num_imcs; p++) {
      prof.profile_bw.pes[i][p] = internal_malloc(sizeof(struct perf_event_attr *) *
                                            profopts.num_profile_bw_events);
      prof.profile_bw.fds[i][p] = internal_malloc(sizeof(int) *
                                            profopts.num_profile_bw_events);
      for(n = 0; n < profopts.num_profile_bw_events; n++) {
        prof.profile_bw.pes[i][p][n] = internal_malloc(sizeof(struct perf_event_attr));
        prof.profile_bw.fds[i][p][n] = 0;
      }
    }
  }

  /* Use libpfm to fill the pe struct */
  sh_get_bw_event();

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
  size_t i, p, n;
  
  /* Allocate room for the events profiling info */
  internal_free(get_bw_prof()->skt);
  
  /* Allocate perf structs */
  for(i = 0; i < profopts.num_profile_skt_cpus; i++) {
    for(p = 0; p < profopts.num_imcs; p++) {
      for(n = 0; n < profopts.num_profile_bw_events; n++) {
        internal_free(prof.profile_bw.pes[i][p][n]);
      }
      internal_free(prof.profile_bw.pes[i][p]);
      internal_free(prof.profile_bw.fds[i][p]);
    }
    internal_free(prof.profile_bw.pes[i]);
    internal_free(prof.profile_bw.fds[i]);
  }
  internal_free(prof.profile_bw.pes);
  internal_free(prof.profile_bw.fds);
}

void *profile_bw(void *a) {
  pthread_setcanceltype(PTHREAD_CANCEL_ASYNCHRONOUS, NULL);

  while(1) { }
}

void profile_bw_interval(int s) {
  long long count;
  size_t i, n, p, total_bw, total_pebs, total_arena_pebs, *total_event_pebs, total_count;
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
    get_bw_skt_prof(i)->current = 0;
    get_bw_skt_prof(i)->current_count = 0;
    for(p = 0; p < profopts.num_imcs; p++) {
      for(n = 0; n < profopts.num_profile_bw_events; n++) {
        ioctl(prof.profile_bw.fds[i][p][n], PERF_EVENT_IOC_DISABLE, 0);
        read(prof.profile_bw.fds[i][p][n], &count, sizeof(long long));
        
        /* Here, the counter should be gathering the number of retired cache
          lines that go through the IMC. */
        get_bw_skt_prof(i)->current += (((double) count) / time);
        get_bw_skt_prof(i)->current_count += count;
      }
    }
  }
  
  if(should_profile_pebs() && should_profile_bw()) {
    /* Clear current values, and add up all profile_pebs values this interval */
    total_pebs = 0;
    total_event_pebs = internal_calloc(prof.profile->num_profile_pebs_events, sizeof(size_t));
    aprof_arr_for(i, aprof) {
      aprof_check_good(i, aprof);
      for(n = 0; n < prof.profile->num_profile_pebs_events; n++) {
        /* total_pebs is the total PEBS counts for this interval
           total_event_pebs is the total PEBS counts for this even for this interval */
        total_pebs += get_pebs_event_prof(i, n)->current;
        total_event_pebs[n] += get_pebs_event_prof(i, n)->current;
      }
    }
    
    /* Add up this interval's bandwidth in tmp */
    total_bw = 0;
    total_count = 0;
    for(i = 0; i < profopts.num_profile_skt_cpus; i++) {
      /* total_bw is the total bandwidth for this interval, for all sockets */
      total_bw += get_bw_skt_prof(i)->current;
      total_count += get_bw_skt_prof(i)->current_count;
    }
    
    /* Spread this interval's bandwidth amongst the arenas, based on
       their profile_pebs values. */
    aprof_arr_for(i, aprof) {
      aprof_check_good(i, aprof);
      total_arena_pebs = 0;
      for(n = 0; n < prof.profile->num_profile_pebs_events; n++) {
        total_arena_pebs += get_pebs_event_prof(i, n)->current;
        /* Per-event bw_relative */
        if(total_event_pebs[n] == 0) {
          get_bw_event_prof(i, n)->current = 0;
          get_bw_event_prof(i, n)->current_count = 0;
        } else {
          get_bw_event_prof(i, n)->current = (((double) get_pebs_event_prof(i, n)->current) / total_event_pebs[n]) * total_bw;
          get_bw_event_prof(i, n)->current_count = ((double) get_pebs_event_prof(i, n)->current / total_event_pebs[n]) * total_count;
        }
      }
      if(total_pebs == 0) {
        get_bw_arena_prof(i)->current = 0;
        get_bw_arena_prof(i)->current_count = 0;
      } else {
        get_bw_arena_prof(i)->current = (((double) total_arena_pebs) / total_pebs) * total_bw;
        get_bw_arena_prof(i)->current_count = (((double) total_arena_pebs) / total_pebs) * total_count;
      }
    }
    internal_free(total_event_pebs);
    
    aprof_arr_for(i, aprof) {
      aprof_check_good(i, aprof);
      get_bw_arena_prof(i)->total += get_bw_arena_prof(i)->current;
      get_bw_arena_prof(i)->total_count += get_bw_arena_prof(i)->current_count;
      for(n = 0; n < prof.profile->num_profile_pebs_events; n++) {
        get_bw_event_prof(i, n)->total += get_bw_event_prof(i, n)->current;
        get_bw_event_prof(i, n)->total_count += get_bw_event_prof(i, n)->current_count;
      }
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
  size_t i, n;
  arena_info *arena;
  arena_profile *aprof;

  /* All we need to do here is maintain the peak */
  for(i = 0; i < profopts.num_profile_skt_cpus; i++) {
    if(get_bw_skt_prof(i)->current > get_bw_skt_prof(i)->peak) {
      get_bw_skt_prof(i)->peak = get_bw_skt_prof(i)->current;
    }
  }
  
  if(should_profile_pebs() && should_profile_bw()) {
    /* We'll have to iterate over all of the arenas and maintain
      their peaks, too. */
    aprof_arr_for(i, aprof) {
      aprof_check_good(i, aprof);
      if(get_bw_arena_prof(i)->current > get_bw_arena_prof(i)->peak) {
        get_bw_arena_prof(i)->peak = get_bw_arena_prof(i)->current;
      }
      for(n = 0; n < prof.profile->num_profile_pebs_events; n++) {
        if(get_bw_event_prof(i, n)->current > get_bw_event_prof(i, n)->peak) {
          get_bw_event_prof(i, n)->peak = get_bw_event_prof(i, n)->current;
        }
      }
    }
  }
}

void profile_bw_skip_interval(int s) {
}
