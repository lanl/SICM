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

void profile_all_arena_init(per_arena_profile_all_info *);
void profile_all_deinit();
void profile_all_init();
void *profile_all(void *);
void profile_all_interval(int);
void profile_all_skip_interval(int);
void profile_all_post_interval(arena_profile *);

/* Uses libpfm to figure out the event we're going to use */
void sh_get_profile_all_event() {
  int err;
  size_t i, n;
  pfm_perf_encode_arg_t pfm;

  pfm_initialize();

  /* Make sure all of the events work. Initialize the pes. */
  for(n = 0; n < profopts.num_profile_all_cpus; n++) {
    for(i = 0; i < prof.profile->num_profile_all_events; i++) {
      memset(prof.profile_all.pes[n][i], 0, sizeof(struct perf_event_attr));
      prof.profile_all.pes[n][i]->size = sizeof(struct perf_event_attr);
      memset(&pfm, 0, sizeof(pfm_perf_encode_arg_t));
      pfm.size = sizeof(pfm_perf_encode_arg_t);
      pfm.attr = prof.profile_all.pes[n][i];

      err = pfm_get_os_event_encoding(prof.profile->profile_all_events[i], PFM_PLM2 | PFM_PLM3, PFM_OS_PERF_EVENT, &pfm);
      if(err != PFM_SUCCESS) {
        fprintf(stderr, "Failed to initialize event '%s'. Aborting.\n", prof.profile->profile_all_events[i]);
        exit(1);
      }

      /* If we're profiling all, set some additional options. */
      if(should_profile_all()) {
        prof.profile_all.pes[n][i]->sample_type = PERF_SAMPLE_TID | PERF_SAMPLE_ADDR;
        prof.profile_all.pes[n][i]->sample_period = profopts.sample_freq;
        prof.profile_all.pes[n][i]->mmap = 1;
        prof.profile_all.pes[n][i]->disabled = 1;
        prof.profile_all.pes[n][i]->exclude_kernel = 1;
        prof.profile_all.pes[n][i]->exclude_hv = 1;
        prof.profile_all.pes[n][i]->precise_ip = 2;
        prof.profile_all.pes[n][i]->task = 1;
        prof.profile_all.pes[n][i]->sample_period = profopts.sample_freq;
      }
    }
  }
}

void profile_all_arena_init(per_arena_profile_all_info *info) {
  size_t i;

  info->events = orig_calloc(prof.profile->num_profile_all_events, sizeof(per_event_profile_all_info));
  for(i = 0; i < prof.profile->num_profile_all_events; i++) {
    info->events[i].total = 0;
    info->events[i].peak = 0;
  }
}

void profile_all_deinit() {
  size_t i, n;

  for(n = 0; n < profopts.num_profile_all_cpus; n++) {
    for(i = 0; i < prof.profile->num_profile_all_events; i++) {
      ioctl(prof.profile_all.fds[n][i], PERF_EVENT_IOC_DISABLE, 0);
    }
  }

  for(n = 0; n < profopts.num_profile_all_cpus; n++) {
    for(i = 0; i < prof.profile->num_profile_all_events; i++) {
      close(prof.profile_all.fds[n][i]);
    }
  }
}

void profile_all_init() {
  size_t i, n;
  pid_t pid;
  int cpu, group_fd;
  unsigned long flags;
  
  prof.profile_all.tid = (unsigned long) syscall(SYS_gettid);
  prof.profile_all.pagesize = (size_t) sysconf(_SC_PAGESIZE);

  /* This array is for storing the per-cpu, per-event data_head values. Instead of calling `poll`, we
     can see if the current data_head value is different from the previous one, and when it is,
     we know we have some new values to read. */
  prof.profile_all.prev_head = orig_malloc(sizeof(uint64_t *) * profopts.num_profile_all_cpus);
  for(n = 0; n < profopts.num_profile_all_cpus; n++) {
    prof.profile_all.prev_head[n] = orig_malloc(sizeof(uint64_t) * prof.profile->num_profile_all_events);
    for(i = 0; i < prof.profile->num_profile_all_events; i++) {
      prof.profile_all.prev_head[n][i] = 0;
    }
  }

  /* Allocate perf structs */
  prof.profile_all.pes = orig_malloc(sizeof(struct perf_event_attr **) * profopts.num_profile_all_cpus);
  prof.profile_all.fds = orig_malloc(sizeof(int *) * profopts.num_profile_all_cpus);
  for(n = 0; n < profopts.num_profile_all_cpus; n++) {
    prof.profile_all.pes[n] = orig_malloc(sizeof(struct perf_event_attr *) * prof.profile->num_profile_all_events);
    prof.profile_all.fds[n] = orig_malloc(sizeof(int) * prof.profile->num_profile_all_events);
    for(i = 0; i < prof.profile->num_profile_all_events; i++) {
      prof.profile_all.pes[n][i] = orig_malloc(sizeof(struct perf_event_attr));
      prof.profile_all.fds[n][i] = 0;
    }
  }

  /* Use libpfm to fill the pe struct */
  sh_get_profile_all_event();

  /* Open all perf file descriptors */
  for(n = 0; n < profopts.num_profile_all_cpus; n++) {
    /* A value of -1 for both `pid` and `cpu` is not valid. */
    if(profopts.profile_all_cpus[n] == -1) {
      pid = 0;
    } else {
      pid = -1;
    }
    cpu = profopts.profile_all_cpus[n];
    group_fd = -1;
    flags = 0;
    for(i = 0; i < prof.profile->num_profile_all_events; i++) {
      prof.profile_all.fds[n][i] = syscall(__NR_perf_event_open, prof.profile_all.pes[n][i], pid, cpu, group_fd, flags);
      if(prof.profile_all.fds[n][i] == -1) {
        fprintf(stderr, "Error opening perf event %d (0x%llx) on cpu %d: %s\n", i, prof.profile_all.pes[n][i]->config, cpu, strerror(errno));
        exit(1);
      }
    }
  }

  /* mmap the perf file descriptors */
  prof.profile_all.metadata = orig_malloc(sizeof(struct perf_event_mmap_page **) * profopts.num_profile_all_cpus);
  for(n = 0; n < profopts.num_profile_all_cpus; n++) {
    prof.profile_all.metadata[n] = orig_malloc(sizeof(struct perf_event_mmap_page *) * prof.profile->num_profile_all_events);
    for(i = 0; i < prof.profile->num_profile_all_events; i++) {
      prof.profile_all.metadata[n][i] = mmap(NULL,
                                          prof.profile_all.pagesize + (prof.profile_all.pagesize * profopts.max_sample_pages),
                                          PROT_READ | PROT_WRITE,
                                          MAP_SHARED,
                                          prof.profile_all.fds[n][i],
                                          0);
      if(prof.profile_all.metadata[n][i] == MAP_FAILED) {
        fprintf(stderr, "Failed to mmap room (%zu bytes) for perf samples. Aborting with:\n%s\n",
                prof.profile_all.pagesize + (prof.profile_all.pagesize * profopts.max_sample_pages), strerror(errno));
        exit(1);
      }
    }
  }

  /* Start the events sampling */
  for(n = 0; n < profopts.num_profile_all_cpus; n++) {
    for(i = 0; i < prof.profile->num_profile_all_events; i++) {
      ioctl(prof.profile_all.fds[n][i], PERF_EVENT_IOC_RESET, 0);
      ioctl(prof.profile_all.fds[n][i], PERF_EVENT_IOC_ENABLE, 0);
    }
  }
}

void *profile_all(void *a) {
  pthread_setcanceltype(PTHREAD_CANCEL_ASYNCHRONOUS, NULL);

  /* Wait for signals */
  while(1) { }
}

/* Just copies the previous value */
void profile_all_skip_interval(int s) {
}

/* Adds up accesses to the arenas */
void profile_all_interval(int s) {
  uint64_t head, tail, buf_size;
  arena_info *arena;
  void *addr;
  char *base, *begin, *end, break_next_site;
  struct sample *sample;
  struct perf_event_header *header;
  int err;
  size_t i, n, x;
  arena_profile *aprof;
  per_event_profile_all_info *per_event_aprof;
  profile_all_info *iprof;
  struct pollfd pfd;
  unsigned index;
  extent_info *extent;
  
  /* Loop over all arenas and clear their accumulators */
  for(i = 0; i < prof.profile->num_profile_all_events; i++) {
    aprof_arr_for(n, aprof) {
      aprof_check_good(n, aprof);
      aprof->profile_all.events[i].current = 0;
    }
  }
  
  iprof = get_profile_all_prof();

  /* Loops over all CPUs */
  for(x = 0; x < profopts.num_profile_all_cpus; x++) {
    /* Loops over all PROFILE_ALL events */
    for(i = 0; i < prof.profile->num_profile_all_events; i++) {

#if 0
      /* Wait for the perf buffer to be ready */
      pfd.fd = prof.profile_all.fds[x][i];
      pfd.events = POLLIN;
      pfd.revents = 0;
      err = poll(&pfd, 1, 1);
      if(err == 0) {
        /* Finished with this interval, there are no ready perf buffers to
         * read from */
        return;
      } else if(err == -1) {
        fprintf(stderr, "Error occurred polling. Aborting.\n");
        exit(1);
      }
#endif

      /* Grab the head. If the head is the same as the previous one, we can just
         move on to the next event; the buffer isn't ready to read yet. */
      head = prof.profile_all.metadata[x][i]->data_head;
      if(head == prof.profile_all.prev_head[x][i]) {
        continue;
      }
      prof.profile_all.prev_head[x][i] = head;

      tail = prof.profile_all.metadata[x][i]->data_tail;
      buf_size = prof.profile_all.pagesize * profopts.max_sample_pages;
      asm volatile("" ::: "memory"); /* Block after reading data_head, per perf docs */

      base = (char *)prof.profile_all.metadata[x][i] + prof.profile_all.pagesize;
      begin = base + tail % buf_size;
      end = base + head % buf_size;

      /* Read all of the samples */
      while(begin <= (end - 8)) {

        header = (struct perf_event_header *)begin;
        if(header->size == 0) {
          break;
        }
        sample = (struct sample *) (begin + 8);
        addr = (void *) (sample->addr);

        if(addr) {
          
          if(pthread_rwlock_rdlock(&tracker.extents_lock) != 0) {
            fprintf(stderr, "Failed to acquire the extents lock in profile_all. Aborting.\n");
            exit(1);
          }
      
          /* Search for which extent it goes into */
          extent_arr_for(tracker.extents, n) {
            extent = &(tracker.extents->arr[n]);
            /* Bail out quickly if either extent address isn't set yet */
            if(!(extent->start) ||
               !(extent->end)) {
              continue;
            }
            /* Now make sure this address is in this extent */
            if((addr >= extent->start) &&
               (addr <= extent->end)) {
              /* This address belongs in this extent, so associate it with
                 the arena that the extent is in. */
              arena = (arena_info *) extent->arena;
              if(!arena) continue;
              aprof_check_good(arena->index, aprof);
              (aprof->profile_all.events[i].current)++;
              iprof->total++;
            }
          }
          
          if(pthread_rwlock_unlock(&tracker.extents_lock) != 0) {
            fprintf(stderr, "Failed to unlock the extents lock in profile_all. Aborting.\n");
            exit(1);
          }
        }

        /* Increment begin by the size of the sample */
        if(((char *)header + header->size) == base + buf_size) {
          begin = base;
        } else {
          begin = begin + header->size;
        }
      }
      
      /* Let perf know that we've read this far */
      prof.profile_all.metadata[x][i]->data_tail = head;
      __sync_synchronize();
    }
  }
}

void profile_all_post_interval(arena_profile *aprof) {
  per_event_profile_all_info *per_event_aprof;
  per_arena_profile_all_info *aprof_all;
  size_t i;

  /* All we need to do here is maintain the peak */
  aprof_all = &(aprof->profile_all);
  for(i = 0; i < prof.profile->num_profile_all_events; i++) {
    per_event_aprof = &(aprof_all->events[i]);
    if(profopts.profile_all_multipliers) {
      per_event_aprof->current *= profopts.profile_all_multipliers[i];
    }
    per_event_aprof->total += per_event_aprof->current;
    if(per_event_aprof->current > per_event_aprof->peak) {
      per_event_aprof->peak = per_event_aprof->current;
    }
  }
}
