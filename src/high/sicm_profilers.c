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

/*************************************************
 *              PROFILE_ALL                      *
 ************************************************/

void profile_all_arena_init(profile_all_info *info) {
  size_t i;

  info->events = orig_calloc(profopts.num_profile_all_events, sizeof(per_event_profile_all_info));
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
  prof.profile_all.pes = orig_malloc(sizeof(struct perf_event_attr *) * profopts.num_profile_all_events);
  prof.profile_all.fds = orig_malloc(sizeof(int) * profopts.num_profile_all_events);
  for(i = 0; i < profopts.num_profile_all_events; i++) {
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
  for(i = 0; i < profopts.num_profile_all_events; i++) {
    prof.profile_all.fds[i] = syscall(__NR_perf_event_open, prof.profile_all.pes[i], pid, cpu, group_fd, flags);
    if(prof.profile_all.fds[i] == -1) {
      fprintf(stderr, "Error opening perf event %d (0x%llx): %s\n", i, prof.profile_all.pes[i]->config, strerror(errno));
      exit(1);
    }
  }

  /* mmap the perf file descriptors */
  prof.profile_all.metadata = orig_malloc(sizeof(struct perf_event_mmap_page *) * profopts.num_profile_all_events);
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

  /* Start the events sampling */
  for(i = 0; i < profopts.num_profile_all_events; i++) {
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

      per_event_profinfo->intervals = (size_t *)orig_realloc(per_event_profinfo->intervals, profinfo->num_intervals * sizeof(size_t));
      if(profinfo->num_intervals == 1) {
        per_event_profinfo->intervals[profinfo->num_intervals - 1] = 0;
      } else {
        per_event_profinfo->intervals[profinfo->num_intervals - 1] = per_event_profinfo->intervals[profinfo->num_intervals - 2];
        per_event_profinfo->total += per_event_profinfo->intervals[profinfo->num_intervals - 1];
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
  profile_info *profinfo;
  per_event_profile_all_info *per_event_profinfo;
  size_t total_samples;
  struct pollfd pfd;

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
            prof.info[arena->index]->profile_all.tmp_accumulator++;
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

  end_interval();
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
    per_event_profinfo->intervals = (size_t *)orig_realloc(per_event_profinfo->intervals, info->num_intervals * sizeof(size_t));
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
    profinfo = prof.info[arena->index];
    if((!profinfo) || (!profinfo->num_intervals)) continue;

    profinfo->profile_rss.intervals = (size_t *)orig_realloc(profinfo->profile_rss.intervals, profinfo->num_intervals * sizeof(size_t));
    if(profinfo->num_intervals == 1) {
      profinfo->profile_rss.intervals[profinfo->num_intervals - 1] = 0;
    } else {
      profinfo->profile_rss.intervals[profinfo->num_intervals - 1] = profinfo->profile_rss.intervals[profinfo->num_intervals - 2];
    }
  }

  pthread_rwlock_unlock(&tracker.extents_lock);

  end_interval();
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
    profinfo = prof.info[arena->index];
    if(!profinfo) continue;
    profinfo->profile_rss.tmp_accumulator = 0;
  }

	/* Iterate over the chunks */
	extent_arr_for(tracker.extents, i) {
		start = (uint64_t) tracker.extents->arr[i].start;
		end = (uint64_t) tracker.extents->arr[i].end;
		arena = (arena_info *) tracker.extents->arr[i].arena;
    if(!arena) continue;
    profinfo = prof.info[arena->index];
    if((!profinfo) || (!profinfo->num_intervals)) continue;

    numpages = (end - start) / prof.profile_rss.pagesize;
		prof.profile_rss.pfndata = (union pfn_t *) orig_realloc(prof.profile_rss.pfndata, numpages * prof.profile_rss.addrsize);

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
	}

  pthread_rwlock_unlock(&tracker.extents_lock);

  end_interval();
}

void profile_rss_post_interval(profile_info *info) {
  profile_rss_info *profinfo;

  profinfo = &(info->profile_rss);

  /* Maintain the peak for this arena */
  if(profinfo->tmp_accumulator > profinfo->peak) {
    profinfo->peak = profinfo->tmp_accumulator;
  }

  /* Store this interval's value */
  profinfo->intervals = (size_t *)orig_realloc(profinfo->intervals, info->num_intervals * sizeof(size_t));
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
    profinfo = (profile_info *) prof.info[arena->index];
    if(!profinfo) continue;

    profinfo->profile_extent_size.tmp_accumulator = 0;
  }

  /* Iterate over the extents and add each of their size to the accumulator */
  extent_arr_for(tracker.extents, i) {
    arena = (arena_info *) tracker.extents->arr[i].arena;
    if(!arena) continue;
    profinfo = (profile_info *) prof.info[arena->index];
    if((!profinfo) || (!profinfo->num_intervals)) continue;

    start = (char *) tracker.extents->arr[i].start;
    end = (char *) tracker.extents->arr[i].end;
    profinfo->profile_extent_size.tmp_accumulator += end - start;
  }

  pthread_rwlock_unlock(&tracker.extents_lock);

  end_interval();
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
    (size_t *)orig_realloc(profinfo->intervals, info->num_intervals * sizeof(size_t));
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
    profinfo = prof.info[arena->index];
    if((!profinfo) || (!profinfo->num_intervals)) continue;

    /* Store this interval */
    profinfo->profile_extent_size.intervals = 
      (size_t *)orig_realloc(profinfo->profile_extent_size.intervals, 
                        profinfo->num_intervals * sizeof(size_t));
    if(profinfo->num_intervals == 1) {
      profinfo->profile_extent_size.intervals[profinfo->num_intervals - 1] = 0;
    } else {
      profinfo->profile_extent_size.intervals[profinfo->num_intervals - 1] = 
        profinfo->profile_extent_size.intervals[profinfo->num_intervals - 2];
    }
  }

  pthread_rwlock_unlock(&tracker.extents_lock);

  end_interval();
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

  end_interval();
}

void profile_allocs_init() {
  tracker.profile_allocs_map = tree_make(addr_t, alloc_info_ptr);
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
    (size_t *)orig_realloc(profinfo->intervals, info->num_intervals * sizeof(size_t));
  profinfo->intervals[info->num_intervals - 1] = profinfo->tmp_accumulator;
}

void profile_allocs_skip_interval(int s) {
  /* TODO */
}

/*************************************************
 *             PROFILE_ONLINE                    *
 ************************************************/
void profile_online_arena_init(profile_online_info *info) {
}

void *profile_online(void *a) {
  pthread_setcanceltype(PTHREAD_CANCEL_ASYNCHRONOUS, NULL);

  while(1) { }
}

size_t get_value(size_t index, size_t event_index) {
  arena_info *arena;
  profile_info *profinfo;
  per_event_profile_all_info *per_event_profinfo;
  size_t value;

  /* Get the profiling information */
  arena = tracker.arenas[index];
  profinfo = prof.info[index];
  per_event_profinfo = &(profinfo->profile_all.events[event_index]);
  if((!arena) || (!profinfo) || (!profinfo->num_intervals)) {
    return 0;
  }

  return per_event_profinfo->total;
}

size_t get_weight(size_t index) {
  arena_info *arena;
  profile_info *profinfo;
  size_t weight;

  /* Get the profiling information */
  arena = tracker.arenas[index];
  profinfo = prof.info[index];
  if((!arena) || (!profinfo) || (!profinfo->num_intervals)) {
    return 0;
  }
  
  /* TODO: Speed this up by setting something up (perhaps an offset into profinfo)
   * in `profile_online_init`.
   */
  if(profopts.should_profile_allocs) {
    return profinfo->profile_allocs.peak;
  } else if(profopts.should_profile_extent_size) {
    return profinfo->profile_extent_size.peak;
  } else if(profopts.should_profile_rss) {
    return profinfo->profile_rss.peak;
  }
}

use_tree(double, size_t);
void profile_online_interval(int s) {
  size_t i, upper_avail, lower_avail,
         value, weight,
         event_index;

  /* Sorted sites */
  tree(double, size_t) sorted_arenas;
  tree_it(double, size_t) it;
  double val_per_weight;

  /* Hotset */
  tree(size_t deviceptr) hotset;
  size_t hotset_value, hotset_weight;
  char break_next_site;

  /* Look at how much the application has consumed on each tier */
  upper_avail = sicm_avail(tracker.upper_device);
  lower_avail = sicm_avail(tracker.lower_device);

  event_index = prof.profile_online.profile_online_event_index;

  printf("Upper_avail: %zu\n", upper_avail);

  if(lower_avail < prof.profile_online.lower_avail_initial) {
    /* The lower tier is now being used, so we need to reconfigure. */
    printf("Lower_avail: %zu\n", lower_avail);

    /* Sort arenas by value/weight in the `sorted_arenas` tree */
    sorted_arenas = tree_make(double, size_t); /* val_per_weight -> arena index */
    for(i = 0; i <= tracker.max_index; i++) {
      value = get_value(i, event_index);
      weight = get_weight(i);
      if((!value) || (!weight)) continue;

      val_per_weight = ((double) value) / ((double) weight);

      /* First see if this value is already in the tree. If so, inch it up just slightly
       * to avoid collisions.
       */
      it = tree_lookup(sorted_arenas, val_per_weight);
      while(tree_it_good(it)) {
        val_per_weight += 0.000000000000000001;
        it = tree_lookup(sorted_arenas, val_per_weight);
      }

      /* Finally insert into the tree */
      tree_insert(sorted_arenas, val_per_weight, i);
    }

    /* Print the sorted sites */
    printf("===== SORTED SITES =====\n");
    it = tree_last(sorted_arenas);
    while(tree_it_good(it)) {
      printf("%zu: %zu/%zu\n", tree_it_val(it), /* Index */
                               get_value(tree_it_val(it), event_index), /* Value */
                               get_weight(tree_it_val(it))); /* Weight */
      tree_it_prev(it);
    }
    printf("===== END SORTED SITES=====\n\n\n");

    /* Iterate over the sites and greedily pack them into the hotset */
    printf("===== HOTSET =====\n");
    hotset_value = 0;
    hotset_weight = 0;
    break_next_site = 0;
    hotset = tree_make(size_t, deviceptr); /* arena index -> online_device */
    it = tree_last(sorted_arenas);
    while(tree_it_good(it)) {
      hotset_value += get_value(tree_it_val(it), event_index);
      hotset_weight += get_weight(tree_it_val(it));
      tree_insert(hotset, tree_it_val(it), upper_device);
      printf("%zu: %zu/%zu\n", tree_it_val(it), /* Index */
                               get_value(tree_it_val(it), event_index), /* Value */
                               get_weight(tree_it_val(it))); /* Weight */
      if(break_next_site) {
        break;
      }
      if(hotset_weight > prof.profile_online.upper_avail_initial) {
        break_next_site = 1;
      }
      tree_it_prev(it);
    }
    printf("\n");
    printf("Total value: %zu\n", hotset_value);
    printf("Packed size: %zu\n", hotset_weight);
    printf("Capacity:    %zd\n", prof.profile_online.upper_avail_initial);
    printf("===== HOTSET =====\n");
  }

  end_interval();
}

void profile_online_init() {
  size_t i;
  char found;

  /* Determine which type of profiling to use to determine weight. Error if none found. */
  /* TODO: Set something up so that we don't have to check this every time `get_weight` is called. */
  if(profopts.should_profile_allocs) {
  } else if(profopts.should_profile_extent_size) {
  } else if(profopts.should_profile_rss) {
  } else {
    fprintf(stderr, "SH_PROFILE_ONLINE requires at least one type of weight profiling. Aborting.\n");
    exit(1);
  }

  /* Look for the event that we're supposed to use for value. Error out if it's not found. */
  if(!profopts.should_profile_all) { 
    fprintf(stderr, "SH_PROFILE_ONLINE requires SH_PROFILE_ALL. Aborting.\n");
    exit(1);
  }
  found = 0;
  for(i = 0; i < profopts.num_profile_all_events; i++) {
    if(strcmp(profopts.profile_all_events[i], profopts.profile_online_event) == 0) {
      found = 1;
      prof.profile_online.profile_online_event_index = i;
      break;
    }
  }
  if(!found) {
    fprintf(stderr, "Event specified in SH_PROFILE_ONLINE_EVENT is not listed in SH_PROFILE_ALL_EVENTS. Aborting.\n");
    exit(1);
  }

  /* Figure out the amount of free memory that we're starting out with */
  prof.profile_online.upper_avail_initial = sicm_avail(tracker.upper_device);
  prof.profile_online.lower_avail_initial = sicm_avail(tracker.lower_device);
  printf("upper_avail_initial: %zu\n", prof.profile_online.upper_avail_initial);
  printf("lower_avail_initial: %zu\n", prof.profile_online.lower_avail_initial);
}

void profile_online_deinit() {
}

void profile_online_post_interval(profile_info *info) {
}

void profile_online_skip_interval(int s) {
}

#if 0
/*************************************************
 *                PROFILE_ONE                    *
 ************************************************/

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

#if 0
void online_reconfigure() {
  size_t i, packed_size, total_value;
  double acc_per_byte;
  tree(double, size_t) sorted_arenas;
  tree(size_t, deviceptr) new_knapsack;
  tree_it(double, size_t) it;
  tree_it(size_t, deviceptr) kit;
  tree_it(int, deviceptr) sit;
  char break_next_site;
  int id;

  printf("===== STARTING RECONFIGURING =====\n");
  /* Sort all sites by accesses/byte */
  sorted_arenas = tree_make(double, size_t); /* acc_per_byte -> arena index */
  packed_size = 0;
  for(i = 0; i <= max_index; i++) {
    if(!arenas[i]) continue;
    if(arenas[i]->peak_rss == 0) continue;
    if(arenas[i]->accesses == 0) continue;
    acc_per_byte = ((double)arenas[i]->accesses) / ((double) arenas[i]->peak_rss);
    it = tree_lookup(sorted_arenas, acc_per_byte);
    while(tree_it_good(it)) {
      /* Inch this site a little higher to avoid collisions in the tree */
      acc_per_byte += 0.000000000000000001;
      it = tree_lookup(sorted_arenas, acc_per_byte);
    }
    tree_insert(sorted_arenas, acc_per_byte, i);
  }

  /* Use a greedy algorithm to pack sites into the knapsack */
  total_value = 0;
  break_next_site = 0;
  new_knapsack = tree_make(size_t, deviceptr); /* arena index -> online_device */
  it = tree_last(sorted_arenas);
  while(tree_it_good(it)) {
    packed_size += arenas[tree_it_val(it)]->peak_rss;
    total_value += arenas[tree_it_val(it)]->accesses;
    tree_insert(new_knapsack, tree_it_val(it), online_device);
    for(id = 0; id < arenas[tree_it_val(it)]->num_alloc_sites; id++) {
      printf("%d ", arenas[tree_it_val(it)]->alloc_sites[id]);
    }
    if(break_next_site) {
      break;
    }
    if(packed_size > online_device_cap) {
      break_next_site = 1;
    }
    tree_it_prev(it);
  }
  printf("\n");
  printf("Total value: %zu\n", total_value);
  printf("Packed size: %zu\n", packed_size);
  printf("Capacity:    %zd\n", online_device_cap);

  /* Get rid of sites that aren't in the new knapsack but are in the old */
  tree_traverse(site_nodes, sit) {
    i = get_arena_index(tree_it_key(sit));
    kit = tree_lookup(new_knapsack, i);
    if(!tree_it_good(kit)) {
      /* The site isn't in the new, so remove it from the upper tier */
      tree_delete(site_nodes, tree_it_key(sit));
      sicm_arena_set_device(arenas[i]->arena, default_device);
      printf("Moving %d out of the MCDRAM\n", tree_it_key(sit));
    }
  }

  /* Add sites that weren't in the old knapsack but are in the new */
  tree_traverse(new_knapsack, kit) {
    for(id = 0; id < arenas[tree_it_key(kit)]->num_alloc_sites; id++) {
      /* Lookup this site in the old knapsack */
      sit = tree_lookup(site_nodes, arenas[tree_it_key(kit)]->alloc_sites[id]);
      if(!tree_it_good(sit)) {
        /* This site is in the new but not the old */
        tree_insert(site_nodes, arenas[tree_it_key(kit)]->alloc_sites[id], online_device);
        sicm_arena_set_device(arenas[tree_it_key(kit)]->arena, online_device);
        printf("Moving %d into the MCDRAM\n", arenas[tree_it_key(kit)]->alloc_sites[id]);
      }
    }
  }

  tree_free(sorted_arenas);
}
#endif
