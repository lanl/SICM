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

void profile_all_arena_init(profile_all_info *);
void profile_all_deinit();
void profile_all_init();
void *profile_all(void *);
void profile_all_interval(int);
void profile_all_skip_interval(int);
void profile_all_post_interval(arena_profile *);

/* Uses libpfm to figure out the event we're going to use */
void sh_get_event() {
  int err;
  size_t i;
  pfm_perf_encode_arg_t pfm;

  pfm_initialize();

  /* Make sure all of the events work. Initialize the pes. */
  for(i = 0; i < prof.profile->num_profile_all_events; i++) {
    memset(prof.profile_all.pes[i], 0, sizeof(struct perf_event_attr));
    prof.profile_all.pes[i]->size = sizeof(struct perf_event_attr);
    memset(&pfm, 0, sizeof(pfm_perf_encode_arg_t));
    pfm.size = sizeof(pfm_perf_encode_arg_t);
    pfm.attr = prof.profile_all.pes[i];

    err = pfm_get_os_event_encoding(prof.profile->profile_all_events[i], PFM_PLM2 | PFM_PLM3, PFM_OS_PERF_EVENT, &pfm);
    if(err != PFM_SUCCESS) {
      fprintf(stderr, "Failed to initialize event '%s'. Aborting.\n", prof.profile->profile_all_events[i]);
      exit(1);
    }

    /* If we're profiling all, set some additional options. */
    if(profopts.should_profile_all) {
      prof.profile_all.pes[i]->sample_type = PERF_SAMPLE_TID | PERF_SAMPLE_ADDR;
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

void profile_all_arena_init(profile_all_info *info) {
  size_t i;

  info->events = orig_calloc(prof.profile->num_profile_all_events, sizeof(per_event_profile_all_info));
  for(i = 0; i < prof.profile->num_profile_all_events; i++) {
    info->events[i].total = 0;
    info->events[i].peak = 0;
    info->events[i].intervals = NULL;
  }
}

void profile_all_deinit() {
  size_t i;

  for(i = 0; i < prof.profile->num_profile_all_events; i++) {
    ioctl(prof.profile_all.fds[i], PERF_EVENT_IOC_DISABLE, 0);
  }

  for(i = 0; i < prof.profile->num_profile_all_events; i++) {
    close(prof.profile_all.fds[i]);
  }
}

void profile_all_init() {
  size_t i;
  pid_t pid;
  int cpu, group_fd;
  unsigned long flags;

  prof.profile_all.tid = (unsigned long) syscall(SYS_gettid);
  prof.profile_all.pagesize = (size_t) sysconf(_SC_PAGESIZE);

  /* Allocate perf structs */
  prof.profile_all.pes = orig_malloc(sizeof(struct perf_event_attr *) * prof.profile->num_profile_all_events);
  prof.profile_all.fds = orig_malloc(sizeof(int) * prof.profile->num_profile_all_events);
  for(i = 0; i < prof.profile->num_profile_all_events; i++) {
    prof.profile_all.pes[i] = orig_malloc(sizeof(struct perf_event_attr));
    prof.profile_all.fds[i] = 0;
  }

  /* Use libpfm to fill the pe struct */
  sh_get_event();

  /* Open all perf file descriptors */
  pid = 0;
  cpu = -1;
  group_fd = -1;
  flags = 0;
  for(i = 0; i < prof.profile->num_profile_all_events; i++) {
    prof.profile_all.fds[i] = syscall(__NR_perf_event_open, prof.profile_all.pes[i], pid, cpu, group_fd, flags);
    if(prof.profile_all.fds[i] == -1) {
      fprintf(stderr, "Error opening perf event %d (0x%llx): %s\n", i, prof.profile_all.pes[i]->config, strerror(errno));
      exit(1);
    }
  }

  /* mmap the perf file descriptors */
  prof.profile_all.metadata = orig_malloc(sizeof(struct perf_event_mmap_page *) * prof.profile->num_profile_all_events);
  for(i = 0; i < prof.profile->num_profile_all_events; i++) {
    prof.profile_all.metadata[i] = mmap(NULL,
                                        prof.profile_all.pagesize + (prof.profile_all.pagesize * profopts.max_sample_pages),
                                        PROT_READ | PROT_WRITE,
                                        MAP_SHARED,
                                        prof.profile_all.fds[i],
                                        0);
    if(prof.profile_all.metadata[i] == MAP_FAILED) {
      fprintf(stderr, "Failed to mmap room (%zu bytes) for perf samples. Aborting with:\n%s\n",
              prof.profile_all.pagesize + (prof.profile_all.pagesize * profopts.max_sample_pages), strerror(errno));
      exit(1);
    }
  }

  /* Start the events sampling */
  for(i = 0; i < prof.profile->num_profile_all_events; i++) {
    ioctl(prof.profile_all.fds[i], PERF_EVENT_IOC_RESET, 0);
    ioctl(prof.profile_all.fds[i], PERF_EVENT_IOC_ENABLE, 0);
  }

}

void *profile_all(void *a) {
  size_t i;

  pthread_setcanceltype(PTHREAD_CANCEL_ASYNCHRONOUS, NULL);

  /* Wait for signals */
  while(1) { }
}

/* Just copies the previous value */
void profile_all_skip_interval(int s) {
  arena_profile *aprof;
  arena_info *arena;
  per_event_profile_all_info *per_event_aprof;
  size_t i, n;

  for(i = 0; i < prof.profile->num_profile_all_events; i++) {
    arena_arr_for(i) {
      prof_check_good(arena, aprof, i);

      per_event_aprof->intervals = (size_t *)orig_realloc(per_event_aprof->intervals, aprof->num_intervals * sizeof(size_t));
      if(aprof->num_intervals == 1) {
        per_event_aprof->intervals[aprof->num_intervals - 1] = 0;
      } else {
        per_event_aprof->intervals[aprof->num_intervals - 1] = per_event_aprof->intervals[aprof->num_intervals - 2];
        per_event_aprof->total += per_event_aprof->intervals[aprof->num_intervals - 1];
      }
    }
  }

  end_interval();
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
  size_t i, n;
  arena_profile *aprof;
  per_event_profile_all_info *per_event_aprof;
  size_t total_samples;
  struct pollfd pfd;

  /* Outer loop loops over the events */
  for(i = 0; i < prof.profile->num_profile_all_events; i++) {

    /* Loops over the arenas */
    total_samples = 0;
    arena_arr_for(n) {
      prof_check_good(arena, aprof, n);

      aprof->profile_all.tmp_accumulator = 0;
    }

#if 0
    /* Wait for the perf buffer to be ready */
    pfd.fd = prof.profile_all.fds[i];
    pfd.events = POLLIN;
    pfd.revents = 0;
    err = poll(&pfd, 1, 1);
    if(err == 0) {
      /* Finished with this interval, there are no ready perf buffers to
       * read from */
      end_interval();
      return;
    } else if(err == -1) {
      fprintf(stderr, "Error occurred polling. Aborting.\n");
      exit(1);
    }
#endif

    /* Get ready to read */
    head = prof.profile_all.metadata[i]->data_head;
    tail = prof.profile_all.metadata[i]->data_tail;
    buf_size = prof.profile_all.pagesize * profopts.max_sample_pages;
    asm volatile("" ::: "memory"); /* Block after reading data_head, per perf docs */

    base = (char *)prof.profile_all.metadata[i] + prof.profile_all.pagesize;
    begin = base + tail % buf_size;
    end = base + head % buf_size;

    printf("===== NEW SAMPLE BATCH =====\n");

    /* Read all of the samples */
    pthread_rwlock_rdlock(&tracker.extents_lock);
    while(begin <= (end - 8)) {

      header = (struct perf_event_header *)begin;
      if(header->size == 0) {
        break;
      }
      sample = (struct sample *) (begin + 8);
      addr = (void *) (sample->addr);

      if(addr && (sample->tid != prof.profile_all.tid)) {
        printf("TID: %lu\n", (unsigned long) sample->tid);
        /* Search for which extent it goes into */
        extent_arr_for(tracker.extents, n) {
          if(!tracker.extents->arr[n].start && !tracker.extents->arr[n].end) continue;
          arena = (arena_info *)tracker.extents->arr[n].arena;
          if((addr >= tracker.extents->arr[n].start) && (addr <= tracker.extents->arr[n].end) && arena) {
            prof.profile->arenas[arena->index]->profile_all.tmp_accumulator++;
            total_samples++;
          }
        }
      }

      /* Increment begin by the size of the sample */
      if(((char *)header + header->size) == base + buf_size) {
        begin = base;
      } else {
        begin = begin + header->size;
      }
    }
    pthread_rwlock_unlock(&tracker.extents_lock);

    fflush(stdout);

    /* Let perf know that we've read this far */
    prof.profile_all.metadata[i]->data_tail = head;
    __sync_synchronize();

  }

  end_interval();
}

void profile_all_post_interval(arena_profile *aprof) {
  per_event_profile_all_info *per_event_aprof;
  profile_all_info *aprof_all;
  size_t i;

  aprof_all = &(aprof->profile_all);

  for(i = 0; i < prof.profile->num_profile_all_events; i++) {
    per_event_aprof = &(aprof_all->events[i]);

    per_event_aprof->total += aprof_all->tmp_accumulator;
    if(aprof_all->tmp_accumulator > per_event_aprof->peak) {
      per_event_aprof->peak = aprof_all->tmp_accumulator;
    }
    /* One size_t per interval for this one event */
    per_event_aprof->intervals = (size_t *)orig_realloc(per_event_aprof->intervals, aprof->num_intervals * sizeof(size_t));
    per_event_aprof->intervals[aprof->num_intervals - 1] = aprof_all->tmp_accumulator;
  }
}
