#pragma once
#define _LARGEFILE64_SOURCE
#include <sys/types.h>
#include "sicm_profile.h"

/* Adds up accesses to the arenas */
static void
get_accesses(int s) {
  uint64_t head, tail, buf_size;
  arena_info *arena;
  void *addr;
  char *base, *begin, *end, break_next_site;
  struct sample *sample;
  struct perf_event_header *header;
  int err;
  size_t i, n;
  profile_info *profinfo;
  size_t total_samples;

  printf("Triggering an interval.\n");

  for(n = 0; n <= tracker.max_index; n++) {
    arena = tracker.arenas[n];
    if(!arena) continue;
    if(arena->num_intervals == 0) {
      /* This is the arena's first interval, make note */
      arena->first_interval = prof.cur_interval;
    }
    arena->num_intervals++;
  }

  /* Outer loop loops over the events */
  for(i = 0; i < profopts.num_events; i++) {

    /* Loops over the arenas */
    total_samples = 0;
    for(n = 0; n <= tracker.max_index; n++) {
      arena = tracker.arenas[n];
      if(!arena) continue;
      arena->accumulator = 0;
    }

    /* Wait for the perf buffer to be ready
    prof.pfd.fd = prof.fds[i];
    prof.pfd.events = POLLIN;
    prof.pfd.revents = 0;
    err = poll(&prof.pfd, 1, 0);
    if(err == 0) {
      return;
    } else if(err == -1) {
      fprintf(stderr, "Error occurred polling. Aborting.\n");
      exit(1);
    } */

    /* Get ready to read */
    head = prof.metadata[i]->data_head;
    tail = prof.metadata[i]->data_tail;
    buf_size = prof.pagesize * profopts.max_sample_pages;
    asm volatile("" ::: "memory"); /* Block after reading data_head, per perf docs */

    base = (char *)prof.metadata[i] + prof.pagesize;
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
          arena = tracker.extents->arr[n].arena;
          if((addr >= tracker.extents->arr[n].start) && (addr <= tracker.extents->arr[n].end) && arena) {
            arena->accumulator++;
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
    prof.metadata[i]->data_tail = head;
    __sync_synchronize();

    for(n = 0; n <= tracker.max_index; n++) {
      arena = tracker.arenas[n];
      /* This check is necessary because an arena could have been created
       * after we added one to the num_intervals up above. num_intervals can't be zero. */
      if((!arena) || (!arena->num_intervals)) continue;
      profinfo = &(arena->profiles[i]);
      profinfo->total += arena->accumulator;
      /* One size_t per interval for this one event */
      profinfo->interval_vals = realloc(profinfo->interval_vals, arena->num_intervals * sizeof(size_t));
      profinfo->interval_vals[arena->num_intervals - 1] = arena->accumulator;
    }
  }
}

static void
get_bandwidth(int s)
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

static void
get_rss(int s) {
	size_t i, n, numpages;
  uint64_t start, end;
  arena_info *arena;
  ssize_t num_read;

  /* Grab the lock for the extents array */
  //pthread_mutex_lock(&arena_lock);
  pthread_rwlock_rdlock(&tracker.extents_lock);

	/* Zero out the RSS values for each arena */
	extent_arr_for(tracker.rss_extents, i) {
    arena = tracker.rss_extents->arr[i].arena;
    if(!arena) continue;
		arena->rss = 0;
	}

	/* Iterate over the chunks */
	extent_arr_for(tracker.rss_extents, i) {
		start = (uint64_t) tracker.rss_extents->arr[i].start;
		end = (uint64_t) tracker.rss_extents->arr[i].end;
		arena = tracker.rss_extents->arr[i].arena;
    if(!arena) continue;

    numpages = (end - start) /prof.pagesize;
		prof.pfndata = (union pfn_t *) realloc(prof.pfndata, numpages * prof.addrsize);

		/* Seek to the starting of this chunk in the pagemap */
		if(lseek64(prof.pagemap_fd, (start / prof.pagesize) * prof.addrsize, SEEK_SET) == ((off64_t) - 1)) {
			close(prof.pagemap_fd);
			fprintf(stderr, "Failed to seek in the PageMap file. Aborting.\n");
			exit(1);
		}

		/* Read in all of the pfns for this chunk */
    if(read(prof.pagemap_fd, prof.pfndata, prof.addrsize * numpages) != (prof.addrsize * numpages)) {
			fprintf(stderr, "Failed to read the PageMap file. Aborting.\n");
			exit(1);
		}

		/* Iterate over them and check them, sum up RSS in arena->rss */
		for(n = 0; n < numpages; n++) {
			if(!(prof.pfndata[n].obj.present)) {
				continue;
		  }
      arena->rss += prof.pagesize;
		}

		/* Maintain the peak for this arena */
		if(arena->rss > arena->peak_rss) {
			arena->peak_rss = arena->rss;
		}
	}

  pthread_rwlock_unlock(&tracker.extents_lock);
  //pthread_mutex_unlock(&arena_lock);
}

void get_allocs(int s) {
}
