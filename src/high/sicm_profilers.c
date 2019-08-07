#include <sys/stat.h>
#include <fcntl.h>
#include "sicm_high.h"

/*************************************************
 *              PROFILE_ALL                      *
 ************************************************/

void profile_all_arena_init(profile_all_info *info) {
  size_t i;

  info->events = calloc(profopts.num_profile_all_events, sizeof(per_event_profile_all_info));
  for(i = 0; i < profopts.num_profile_all_events; i++) {
    info->events[i].total = 0;
    info->events[i].peak = 0;
    info->events[i].intervals = NULL;
  }
}

void profile_all_deinit() {
  size_t i;

  for(i = 0; i < profopts.num_profile_all_events; i++) {
    ioctl(prof.profile_all.fds[i], PERF_EVENT_IOC_DISABLE, 0);
  }

  for(i = 0; i < profopts.num_profile_all_events; i++) {
    close(prof.profile_all.fds[i]);
  }
}

void profile_all_init() {
  size_t i;
  pid_t pid;
  int cpu, group_fd;
  unsigned long flags;

  prof.profile_all.pagesize = (size_t) sysconf(_SC_PAGESIZE);

  /* Allocate perf structs */
  prof.profile_all.pes = malloc(sizeof(struct perf_event_attr *) * profopts.num_profile_all_events);
  prof.profile_all.fds = malloc(sizeof(int) * profopts.num_profile_all_events);
  for(i = 0; i < profopts.num_profile_all_events; i++) {
    prof.profile_all.pes[i] = malloc(sizeof(struct perf_event_attr));
    prof.profile_all.fds[i] = 0;
  }

  /* Use libpfm to fill the pe struct */
  sh_get_event();

  /* Open all perf file descriptors */
	pid = 0;
	cpu = -1;
	group_fd = -1;
	flags = 0;
  for(i = 0; i < profopts.num_profile_all_events; i++) {
    prof.profile_all.fds[i] = syscall(__NR_perf_event_open, prof.profile_all.pes[i], pid, cpu, group_fd, flags);
    if(prof.profile_all.fds[i] == -1) {
      fprintf(stderr, "Error opening perf event %d (0x%llx): %s\n", i, prof.profile_all.pes[i]->config, strerror(errno));
      exit(1);
    }
  }

  /* mmap the perf file descriptors */
  prof.profile_all.metadata = malloc(sizeof(struct perf_event_mmap_page *) * profopts.num_profile_all_events);
  for(i = 0; i < profopts.num_profile_all_events; i++) {
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
}

void *profile_all(void *a) {
  size_t i;

  pthread_setcanceltype(PTHREAD_CANCEL_ASYNCHRONOUS, NULL);

  /* Start the events sampling */
  for(i = 0; i < profopts.num_profile_all_events; i++) {
    ioctl(prof.profile_all.fds[i], PERF_EVENT_IOC_RESET, 0);
    ioctl(prof.profile_all.fds[i], PERF_EVENT_IOC_ENABLE, 0);
  }

  /* Wait for signals */
  while(1) { }
}

/* Just copies the previous value */
void profile_all_skip_interval(int s) {
  profile_info *profinfo;
  arena_info *arena;
  per_event_profile_all_info *per_event_profinfo;
  size_t i, n;

  for(i = 0; i < profopts.num_profile_all_events; i++) {
    for(n = 0; n <= tracker.max_index; n++) {
      arena = tracker.arenas[n];
      profinfo = prof.info[n];
      per_event_profinfo = &(profinfo->profile_all.events[i]);
      if((!arena) || (!profinfo) || (!profinfo->num_intervals)) continue;

      per_event_profinfo->intervals = (size_t *)realloc(per_event_profinfo->intervals, profinfo->num_intervals * sizeof(size_t));
      if(profinfo->num_intervals == 1) {
        per_event_profinfo->intervals[profinfo->num_intervals - 1] = 0;
      } else {
        per_event_profinfo->intervals[profinfo->num_intervals - 1] = per_event_profinfo->intervals[profinfo->num_intervals - 2];
        per_event_profinfo->total += per_event_profinfo->intervals[profinfo->num_intervals - 1];
      }
    }
  }

  end_interval(s);
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
  profile_info *profinfo;
  per_event_profile_all_info *per_event_profinfo;
  size_t total_samples;

  /* Outer loop loops over the events */
  for(i = 0; i < profopts.num_profile_all_events; i++) {

    /* Loops over the arenas */
    total_samples = 0;
    for(n = 0; n <= tracker.max_index; n++) {
      profinfo = prof.info[n];
      if(!profinfo) continue;
      profinfo->profile_all.tmp_accumulator = 0;
    }

#if 0
    /* Wait for the perf buffer to be ready */
    prof.pfd.fd = prof.fds[i];
    prof.pfd.events = POLLIN;
    prof.pfd.revents = 0;
    err = poll(&prof.pfd, 1, 0);
    if(err == 0) {
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

    /* Read all of the samples */
    pthread_rwlock_rdlock(&tracker.extents_lock);
    while(begin <= (end - 8)) {

      header = (struct perf_event_header *)begin;
      if(header->size == 0) {
        break;
      }
      sample = (struct sample *) (begin + 8);
      addr = (void *) (sample->addr);

      if(addr) {
        /* Search for which extent it goes into */
        extent_arr_for(tracker.extents, n) {
          if(!tracker.extents->arr[n].start && !tracker.extents->arr[n].end) continue;
          arena = (arena_info *)tracker.extents->arr[n].arena;
          if((addr >= tracker.extents->arr[n].start) && (addr <= tracker.extents->arr[n].end) && arena) {
            ((profile_info *)arena->info)->profile_all.tmp_accumulator++;
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

    /* Let perf know that we've read this far */
    prof.profile_all.metadata[i]->data_tail = head;
    __sync_synchronize();

  }

  end_interval(s);
}

void profile_all_post_interval(profile_info *info) {
  per_event_profile_all_info *per_event_profinfo;
  profile_all_info *profinfo;
  size_t i;

  profinfo = &(info->profile_all);

  for(i = 0; i < profopts.num_profile_all_events; i++) {
    per_event_profinfo = &(profinfo->events[i]);

    per_event_profinfo->total += profinfo->tmp_accumulator;
    if(profinfo->tmp_accumulator > per_event_profinfo->peak) {
      per_event_profinfo->peak = profinfo->tmp_accumulator;
    }
    /* One size_t per interval for this one event */
    per_event_profinfo->intervals = (size_t *)realloc(per_event_profinfo->intervals, info->num_intervals * sizeof(size_t));
    per_event_profinfo->intervals[info->num_intervals - 1] = profinfo->tmp_accumulator;
  }
}

/*************************************************
 *              PROFILE_RSS                      *
 ************************************************/

void profile_rss_arena_init(profile_rss_info *info) {
  info->peak = 0;
  info->intervals = NULL;
}

void profile_rss_deinit() {
  close(prof.profile_rss.pagemap_fd);
}

void profile_rss_init() {
  prof.profile_rss.pagemap_fd = open("/proc/self/pagemap", O_RDONLY);
  if (prof.profile_rss.pagemap_fd < 0) {
    fprintf(stderr, "Failed to open /proc/self/pagemap. Aborting.\n");
    exit(1);
  }
  prof.profile_rss.pfndata = NULL;
  prof.profile_rss.addrsize = sizeof(uint64_t);
  prof.profile_rss.pagesize = (size_t) sysconf(_SC_PAGESIZE);
}

void *profile_rss(void *a) {
  pthread_setcanceltype(PTHREAD_CANCEL_ASYNCHRONOUS, NULL);

  while(1) { }
}

/* Just copies the previous value */
void profile_rss_skip_interval(int s) {
  profile_info *profinfo;
  arena_info *arena;
  size_t i;

  pthread_rwlock_rdlock(&tracker.extents_lock);

  extent_arr_for(tracker.extents, i) {
    arena = (arena_info *) tracker.extents->arr[i].arena;
    if(!arena) continue;
    profinfo = (profile_info *) arena->info;
    if((!profinfo) || (!profinfo->num_intervals)) continue;

    profinfo->profile_rss.intervals = (size_t *)realloc(profinfo->profile_rss.intervals, profinfo->num_intervals * sizeof(size_t));
    if(profinfo->num_intervals == 1) {
      profinfo->profile_rss.intervals[profinfo->num_intervals - 1] = 0;
    } else {
      profinfo->profile_rss.intervals[profinfo->num_intervals - 1] = profinfo->profile_rss.intervals[profinfo->num_intervals - 2];
    }
  }

  pthread_rwlock_unlock(&tracker.extents_lock);

  end_interval(s);
}

void profile_rss_interval(int s) {
	size_t i, n, numpages;
  uint64_t start, end;
  arena_info *arena;
  ssize_t num_read;
  profile_info *profinfo;

  /* Grab the lock for the extents array */
  pthread_rwlock_rdlock(&tracker.extents_lock);

  /* Zero out the accumulator for each arena */
	extent_arr_for(tracker.extents, i) {
    arena = (arena_info *) tracker.extents->arr[i].arena;
    if(!arena) continue;
    profinfo = (profile_info *) arena->info;
    if(!profinfo) continue;
    profinfo->profile_rss.tmp_accumulator = 0;
  }

	/* Iterate over the chunks */
	extent_arr_for(tracker.extents, i) {
		start = (uint64_t) tracker.extents->arr[i].start;
		end = (uint64_t) tracker.extents->arr[i].end;
		arena = (arena_info *) tracker.extents->arr[i].arena;
    if(!arena) continue;
    profinfo = (profile_info *) arena->info;
    if((!profinfo) || (!profinfo->num_intervals)) continue;

    numpages = (end - start) / prof.profile_rss.pagesize;
		prof.profile_rss.pfndata = (union pfn_t *) realloc(prof.profile_rss.pfndata, numpages * prof.profile_rss.addrsize);

		/* Seek to the starting of this chunk in the pagemap */
		if(lseek64(prof.profile_rss.pagemap_fd, (start / prof.profile_rss.pagesize) * prof.profile_rss.addrsize, SEEK_SET) == ((__off64_t) - 1)) {
			close(prof.profile_rss.pagemap_fd);
			fprintf(stderr, "Failed to seek in the PageMap file. Aborting.\n");
			exit(1);
		}

		/* Read in all of the pfns for this chunk */
    num_read = read(prof.profile_rss.pagemap_fd, prof.profile_rss.pfndata, prof.profile_rss.addrsize * numpages);
    if(num_read == -1) {
      fprintf(stderr, "Failed to read from PageMap file. Aborting: %d, %s\n", errno, strerror(errno));
      exit(1);
		} else if(num_read < prof.profile_rss.addrsize * numpages) {
      printf("WARNING: Read less bytes than expected.\n");
      continue;
    }

		/* Iterate over them and check them, sum up RSS in arena->rss */
		for(n = 0; n < numpages; n++) {
			if(!(prof.profile_rss.pfndata[n].obj.present)) {
				continue;
		  }
      profinfo->profile_rss.tmp_accumulator += prof.profile_rss.pagesize;
		}

#if 0
		/* Maintain the peak for this arena */
		if(profinfo->profile_rss.tmp_accumulator > profinfo->profile_rss.peak) {
		  profinfo->profile_rss.peak = profinfo->profile_rss.tmp_accumulator;
		}

    /* Store this interval's value */
    profinfo->profile_rss.intervals = (size_t *)realloc(profinfo->profile_rss.intervals, profinfo->num_intervals * sizeof(size_t));
    profinfo->profile_rss.intervals[profinfo->num_intervals - 1] = profinfo->profile_rss.tmp_accumulator;
#endif
	}

  pthread_rwlock_unlock(&tracker.extents_lock);

  end_interval(s);
}

void profile_rss_post_interval(profile_info *info) {
  profile_rss_info *profinfo;

  profinfo = &(info->profile_rss);

  /* Maintain the peak for this arena */
  if(profinfo->tmp_accumulator > profinfo->peak) {
    profinfo->peak = profinfo->tmp_accumulator;
  }

  /* Store this interval's value */
  profinfo->intervals = (size_t *)realloc(profinfo->intervals, info->num_intervals * sizeof(size_t));
  profinfo->intervals[info->num_intervals - 1] = profinfo->tmp_accumulator;
}

/*************************************************
 *            PROFILE_EXTENT_SIZE                *
 ************************************************/

void *profile_extent_size(void *a) {
  pthread_setcanceltype(PTHREAD_CANCEL_ASYNCHRONOUS, NULL);

  while(1) { }
}

void profile_extent_size_interval(int s) {
  profile_info *profinfo;
  arena_info *arena;
  size_t i;
  char *start, *end;

  pthread_rwlock_rdlock(&tracker.extents_lock);
  
  /* Zero out the accumulator for each arena */
  extent_arr_for(tracker.extents, i) {
    arena = (arena_info *) tracker.extents->arr[i].arena;
    if(!arena) continue;
    profinfo = (profile_info *) arena->info;
    if(!profinfo) continue;

    profinfo->profile_extent_size.tmp_accumulator = 0;
  }

  /* Iterate over the extents and add each of their size to the accumulator */
  extent_arr_for(tracker.extents, i) {
    arena = (arena_info *) tracker.extents->arr[i].arena;
    if(!arena) continue;
    profinfo = (profile_info *) arena->info;
    if((!profinfo) || (!profinfo->num_intervals)) continue;

    start = (char *) tracker.extents->arr[i].start;
    end = (char *) tracker.extents->arr[i].end;
    profinfo->profile_extent_size.tmp_accumulator += end - start;
  }

#if 0
  /* Now go through each arena one last time */
  extent_arr_for(tracker.extents, i) {
    arena = (arena_info *) tracker.extents->arr[i].arena;
    if(!arena) continue;
    profinfo = (profile_info *) arena->info;
    if((!profinfo) || (!profinfo->num_intervals)) continue;

    /* Maintain peak */
    if(profinfo->profile_extent_size.tmp_accumulator > profinfo->profile_extent_size.peak) {
      profinfo->profile_extent_size.peak = profinfo->profile_extent_size.tmp_accumulator;
    }

    /* Store this interval */
    profinfo->profile_extent_size.intervals = 
      (size_t *)realloc(profinfo->profile_extent_size.intervals, 
                        profinfo->num_intervals * sizeof(size_t));
    profinfo->profile_extent_size.intervals[profinfo->num_intervals - 1] = 
      profinfo->profile_extent_size.tmp_accumulator;
  }
#endif

  pthread_rwlock_unlock(&tracker.extents_lock);

  end_interval(s);
}

void profile_extent_size_post_interval(profile_info *info) {
  profile_extent_size_info *profinfo;

  profinfo = &(info->profile_extent_size);

  /* Maintain peak */
  if(profinfo->tmp_accumulator > profinfo->peak) {
    profinfo->peak = profinfo->tmp_accumulator;
  }

  /* Store this interval */
  profinfo->intervals = 
    (size_t *)realloc(profinfo->intervals, info->num_intervals * sizeof(size_t));
  profinfo->intervals[info->num_intervals - 1] = profinfo->tmp_accumulator;
}

/* Just copies previous values along */
void profile_extent_size_skip_interval(int s) {
  profile_info *profinfo;
  arena_info *arena;
  size_t i;

  pthread_rwlock_rdlock(&tracker.extents_lock);
  
  extent_arr_for(tracker.extents, i) {
    arena = (arena_info *) tracker.extents->arr[i].arena;
    if(!arena) continue;
    profinfo = (profile_info *) arena->info;
    if((!profinfo) || (!profinfo->num_intervals)) continue;

    /* Store this interval */
    profinfo->profile_extent_size.intervals = 
      (size_t *)realloc(profinfo->profile_extent_size.intervals, 
                        profinfo->num_intervals * sizeof(size_t));
    if(profinfo->num_intervals == 1) {
      profinfo->profile_extent_size.intervals[profinfo->num_intervals - 1] = 0;
    } else {
      profinfo->profile_extent_size.intervals[profinfo->num_intervals - 1] = 
        profinfo->profile_extent_size.intervals[profinfo->num_intervals - 2];
    }
  }

  pthread_rwlock_unlock(&tracker.extents_lock);

  end_interval(s);
}

void profile_extent_size_init() {
}

void profile_extent_size_deinit() {
}

void profile_extent_size_arena_init(profile_extent_size_info *info) {
  info->peak = 0;
  info->intervals = NULL;
  info->tmp_accumulator = 0;
}

/*************************************************
 *              PROFILE_ALLOCS                   *
 ************************************************/

void profile_allocs_arena_init(profile_allocs_info *info) {
  info->peak = 0;
  info->intervals = NULL;
  info->tmp_accumulator = 0;
}

void *profile_allocs(void *a) {
  pthread_setcanceltype(PTHREAD_CANCEL_ASYNCHRONOUS, NULL);

  while(1) { }
}

void profile_allocs_interval(int s) {
  arena_info *arena;
  profile_info *profinfo;
  size_t i;

  /* Iterate over the arenas and set their size to the tmp_accumulator */
  for(i = 0; i <= tracker.max_index; i++) {
    arena = tracker.arenas[i];
    profinfo = prof.info[i];
    if((!arena) || (!profinfo) || (!profinfo->num_intervals)) continue;

    profinfo->profile_allocs.tmp_accumulator = arena->size;
  }

  end_interval(s);
}

void profile_allocs_init() {
  pthread_rwlock_init(&tracker.profile_allocs_map_lock, NULL);
}

void profile_allocs_deinit() {
}

void profile_allocs_post_interval(profile_info *info) {
  profile_allocs_info *profinfo;

  profinfo = &(info->profile_allocs);

  /* Maintain peak */
  if(profinfo->tmp_accumulator > profinfo->peak) {
    profinfo->peak = profinfo->tmp_accumulator;
  }

  /* Store this interval */
  profinfo->intervals = 
    (size_t *)realloc(profinfo->intervals, info->num_intervals * sizeof(size_t));
  profinfo->intervals[info->num_intervals - 1] = profinfo->tmp_accumulator;
}

void profile_allocs_skip_interval(int s) {
  /* TODO */
}

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
