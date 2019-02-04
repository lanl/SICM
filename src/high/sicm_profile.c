#define _LARGEFILE64_SOURCE
#include "sicm_high.h"
#include "sicm_profile.h"
#include "sicm_impl.h"
#include <sys/types.h>
#include <unistd.h>
#include <fcntl.h>

profile_thread prof;
use_tree(double, size_t);
use_tree(size_t, deviceptr);

const char* accesses_event_strs[] = {
  "MEM_LOAD_UOPS_RETIRED.L3_MISS",
  "MEM_UOPS_RETIRED.L2_MISS_LOADS",
  NULL
};

const char* bandwidth_event_strs[] = {
  "UNC_M_CAS_COUNT:ALL",
  NULL
};

int num_events;

/* Uses libpfm to figure out the event we're going to use */
void sh_get_event() {
  const char **event_strs;
  char **event, *buf;
  int err, found, i;

  pfm_initialize();
  prof.pfm = malloc(sizeof(pfm_perf_encode_arg_t));

  /* Get the array of event strs that we want to use */
  if(should_profile_all) {
    event_strs = accesses_event_strs;
  } else if(should_profile_one) {
    event_strs = bandwidth_event_strs;
  }

  /* Iterate through the array of event strs and see which one works. 
   * For should_profile_one, just use the first given IMC. */
  if(should_profile_one && profile_one_event) {
    event = &profile_one_event;
    printf("Using a user-specified event: %s\n", profile_one_event);
  } else {
    event = event_strs;
    found = 0;
    while(*event != NULL) {
      memset(prof.pes[0], 0, sizeof(struct perf_event_attr));
      prof.pes[0]->size = sizeof(struct perf_event_attr);
      memset(prof.pfm, 0, sizeof(pfm_perf_encode_arg_t));
      prof.pfm->size = sizeof(pfm_perf_encode_arg_t);
      prof.pfm->attr = prof.pes[0];
      err = pfm_get_os_event_encoding(*event, PFM_PLM2 | PFM_PLM3, PFM_OS_PERF_EVENT, prof.pfm);
      if(err == PFM_SUCCESS) {
        printf("Using event: %s (0x%llx)\n", *event, prof.pes[0]->config);
        found = 1;
        break;
      }
      event++;
    }
    if(!found) {
      fprintf(stderr, "Couldn't find an appropriate event to use. Aborting.\n");
      exit(1);
    }
  }

  /* If should_profile_all, we're using PEBS and only one event */
  if(should_profile_all) {
    prof.pes[0]->sample_type = PERF_SAMPLE_ADDR;
    prof.pes[0]->sample_period = sample_freq;
    prof.pes[0]->mmap = 1;
    prof.pes[0]->disabled = 1;
    prof.pes[0]->exclude_kernel = 1;
    prof.pes[0]->exclude_hv = 1;
    prof.pes[0]->precise_ip = 2;
    prof.pes[0]->task = 1;
    prof.pes[0]->sample_period = sample_freq;

  /* If we're doing memory bandwidth sampling, initialize the other IMCs with the same event */
  } else if(should_profile_one) {
    buf = calloc(max_imc_len + max_event_len + 3, sizeof(char));
    for(i = 0; i < num_imcs; i++) {

      /* Initialize the pe struct */
      memset(prof.pes[i], 0, sizeof(struct perf_event_attr));
      prof.pes[i]->size = sizeof(struct perf_event_attr);
      memset(prof.pfm, 0, sizeof(pfm_perf_encode_arg_t));
      prof.pfm->size = sizeof(pfm_perf_encode_arg_t);
      prof.pfm->attr = prof.pes[i];

      /* Construct the event string from the IMC and the event */
      memset(buf, 0, sizeof(char) * (max_imc_len + max_event_len + 3));
      sprintf(buf, "%s::%s", imcs[i], *event);
      printf("Using full event string: %s\n", buf);

      err = pfm_get_os_event_encoding(buf, PFM_PLM2 | PFM_PLM3, PFM_OS_PERF_EVENT, prof.pfm);
      if(err != PFM_SUCCESS) {
        fprintf(stderr, "Failed to use libpfm with event string %s. Aborting.\n", buf);
        exit(1);
      }
    }
    free(buf);
  }
}

int sh_should_stop() {
  switch(pthread_mutex_trylock(&prof.mtx)) {
    case 0:
      pthread_mutex_unlock(&prof.mtx);
      return 1;
    case EBUSY:
      return 0;
  }
  return 1;
}

void sh_start_profile_thread() {
  size_t i;

  /* All of this initialization HAS to happen in the main SICM thread.
   * If it's not, the `perf_event_open` system call won't profile
   * the current thread, but instead will only profile the thread that
   * it was run in.
   */

  num_events = 0;
  if(should_profile_all) {
    num_events = 1;
  } else if(should_profile_one) {
    num_events = num_imcs;
  }

  prof.pagesize = (size_t) sysconf(_SC_PAGESIZE);

  /* Allocate perf structs */
  prof.pes = malloc(sizeof(struct perf_event_attr *) * num_events);
  prof.fds = malloc(sizeof(int) * num_events);
  for(i = 0; i < num_events; i++) {
    prof.pes[i] = malloc(sizeof(struct perf_event_attr));
    prof.fds[i] = 0;
  }

  /* Use libpfm to fill the pe struct */
  if(should_profile_all || should_profile_one) {
    sh_get_event();
  }

  /* Open the perf file descriptor */
  if(should_profile_all) {
    prof.fds[0] = syscall(__NR_perf_event_open, prof.pes[0], 0, -1, -1, 0);
    if (prof.fds[0] == -1) {
      fprintf(stderr, "Error opening perf event 0x%llx.\n", prof.pes[0]->config);
      strerror(errno);
      exit(EXIT_FAILURE);
    }
  } else if(should_profile_one) {
    for(i = 0; i < num_events; i++) {
      prof.fds[i] = syscall(__NR_perf_event_open, prof.pes[i], -1, 0, -1, 0);
      if (prof.fds[i] == -1) {
        fprintf(stderr, "Error opening perf event %d (0x%llx).\n", i, prof.pes[i]->config);
        printf("%d\n", errno);
        strerror(errno);
        exit(EXIT_FAILURE);
      }
    }
  }

  if(should_profile_rss) {
    prof.pagemap_fd = open("/proc/self/pagemap", O_RDONLY);
    if (prof.pagemap_fd < 0) {
      fprintf(stderr, "Failed to open /proc/self/pagemap. Aborting.\n");
      exit(1);
    }
  }

  /* Start the profiling threads */
  pthread_mutex_init(&prof.mtx, NULL);
  pthread_mutex_lock(&prof.mtx);
  if(should_profile_all) {
    pthread_create(&prof.profile_all_id, NULL, &profile_all, NULL);
  } else if(should_profile_one) {
    pthread_create(&prof.profile_one_id, NULL, &profile_one, NULL);
  }
  if(should_profile_rss) {
    pthread_create(&prof.profile_rss_id, NULL, &profile_rss, NULL);
  }
}

void sh_stop_profile_thread() {
  size_t i, associated;

  /* Stop the actual sampling */
  for(i = 0; i < num_events; i++) {
    ioctl(prof.fds[i], PERF_EVENT_IOC_DISABLE, 0);
  }

  /* Stop the timers and join the threads */
  pthread_mutex_unlock(&prof.mtx);
  if(should_profile_all) {
    pthread_join(prof.profile_all_id, NULL);
  } else if(should_profile_one) {
    pthread_join(prof.profile_one_id, NULL);
  }
  if(should_profile_rss) {
    pthread_join(prof.profile_rss_id, NULL);
  }

  for(i = 0; i < num_events; i++) {
    close(prof.fds[i]);
  }

  if(should_profile_all) {
    printf("===== PEBS RESULTS =====\n");
    associated = 0;
    for(i = 0; i <= max_index; i++) {
      if(!arenas[i]) continue;
      associated += arenas[i]->accesses;
      printf("Site %u:\n", arenas[i]->id);
      printf("  Accesses: %zu\n", arenas[i]->accesses);
      if(should_profile_rss) {
        printf("  Peak RSS: %zu\n", arenas[i]->peak_rss);
      }
    }
    printf("Totals: %zu / %zu\n", associated, prof.total);
    printf("===== END PEBS RESULTS =====\n");
  } else if(should_profile_one) {
    printf("===== MBI RESULTS FOR SITE %u =====\n", should_profile_one);
    printf("Average bandwidth: %.1f MB/s\n", prof.running_avg);
    if(should_profile_rss) {
      printf("Peak RSS: %zu\n", arenas[should_profile_one]->peak_rss);
    }
    printf("===== END MBI RESULTS =====\n");
  } else if(should_profile_rss) {
    printf("===== RSS RESULTS =====\n");
    for(i = 0; i <= max_index; i++) {
      if(!arenas[i]) continue;
      printf("Site %u:\n", arenas[i]->id);
      if(should_profile_rss) {
        printf("  Peak RSS: %zu\n", arenas[i]->peak_rss);
      }
    }
    printf("===== END RSS RESULTS =====\n");
  }
}

/* Adds up accesses to the arenas */
static void
get_accesses() {
  uint64_t head, tail, buf_size;
  arena_info *arena;
  void *addr;
  char *base, *begin, *end, break_next_site;
  size_t i, packed_size, total_value;
  struct sample *sample;
  struct perf_event_header *header;
  double acc_per_byte;
  tree(double, size_t) sorted_arenas;
  tree(size_t, deviceptr) new_knapsack;
  tree_it(double, size_t) it;
  tree_it(size_t, deviceptr) kit;
  tree_it(unsigned, deviceptr) sit;
  int err;

  /* Wait for the perf buffer to be ready */
  prof.pfd.fd = prof.fds[0];
  prof.pfd.events = POLLIN;
  prof.pfd.revents = 0;
  err = poll(&prof.pfd, 1, 0);
  if(err == 0) {
    return;
  } else if(err == -1) {
    fprintf(stderr, "Error occurred polling. Aborting.\n");
    exit(1);
  }

  /* Get ready to read */
  head = prof.metadata->data_head;
  tail = prof.metadata->data_tail;
  buf_size = prof.pagesize * max_sample_pages;
  asm volatile("" ::: "memory"); /* Block after reading data_head, per perf docs */

  base = (char *)prof.metadata + prof.pagesize;
  begin = base + tail % buf_size;
  end = base + head % buf_size;

  /* Read all of the samples */
  pthread_rwlock_rdlock(&extents_lock);
  while(begin != end) {

    header = (struct perf_event_header *)begin;
    if(header->size == 0) {
      break;
    }
    sample = (struct sample *) (begin + 8);
    addr = (void *) (sample->addr);

    if(addr) {
      prof.total++;
      /* Search for which extent it goes into */
      extent_arr_for(extents, i) {
        if(!extents->arr[i].start && !extents->arr[i].end) continue;
        arena = extents->arr[i].arena;
        if((addr >= extents->arr[i].start) && (addr <= extents->arr[i].end)) {
          arena->accesses++;
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
  pthread_rwlock_unlock(&extents_lock);

  /* Let perf know that we've read this far */
  prof.metadata->data_tail = head;
  __sync_synchronize();

  if(should_profile_online) {
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
      printf("%zu ", arenas[tree_it_val(it)]->id);
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
        printf("Moving %u out of the MCDRAM\n", tree_it_key(sit));
      }
    }

    /* Add sites that weren't in the old knapsack but are in the new */
    tree_traverse(new_knapsack, kit) {
      /* Lookup this site in the old knapsack */
      sit = tree_lookup(site_nodes, arenas[tree_it_key(kit)]->id);
      if(!tree_it_good(sit)) {
        /* This site is in the new but not the old */
        tree_insert(site_nodes, arenas[tree_it_key(kit)]->id, online_device);
        sicm_arena_set_device(arenas[tree_it_key(kit)]->arena, online_device);
        printf("Moving %u into the MCDRAM\n", arenas[tree_it_key(kit)]->id);
      }
    }

    tree_free(sorted_arenas);
  }
}

static void
get_bandwidth()
{
  float count_f, total;
  long long count;
  int num, i;
  struct itimerspec it;

  /* Stop the counter and read the value if it has been at least a second */
  total = 0;
  for(i = 0; i < num_events; i++) {
    ioctl(prof.fds[i], PERF_EVENT_IOC_DISABLE, 0);
    read(prof.fds[i], &count, sizeof(long long));
    count_f = (float) count * 64 / 1024 / 1024;
    total += count_f;

    /* Start it back up again */
    ioctl(prof.fds[i], PERF_EVENT_IOC_RESET, 0);
    ioctl(prof.fds[i], PERF_EVENT_IOC_ENABLE, 0);
  }

  printf("%.2f MB/s\n", total);
  
  /* Calculate the running average */
  prof.num_intervals++;
  prof.running_avg = ((prof.running_avg * (prof.num_intervals - 1)) + total) / prof.num_intervals;
}

static void
get_rss() {
	size_t i, n, numpages;
  uint64_t start, end;
  arena_info *arena;
  ssize_t num_read;

  /* Grab the lock for the extents array */
  pthread_rwlock_rdlock(&extents_lock);

	/* Zero out the RSS values for each arena */
	extent_arr_for(rss_extents, i) {
    arena = rss_extents->arr[i].arena;
		arena->rss = 0;
	}

	/* Iterate over the chunks */
	extent_arr_for(rss_extents, i) {
		start = (uint64_t) rss_extents->arr[i].start;
		end = (uint64_t) rss_extents->arr[i].end;
		arena = rss_extents->arr[i].arena;

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
  pthread_rwlock_unlock(&extents_lock);
}

void *profile_rss(void *a) {
  struct timespec timer;

  prof.pagesize = (size_t) sysconf(_SC_PAGESIZE);

  prof.pfndata = NULL;
  prof.addrsize = sizeof(uint64_t);

  timer.tv_sec = 1;
  timer.tv_nsec = 0;

  while(!sh_should_stop()) {
    get_rss();
    nanosleep(&timer, NULL);
  }
}

void *profile_all(void *a) {
  struct timespec timer;

  /* mmap the file */
  prof.metadata = mmap(NULL, prof.pagesize + (prof.pagesize * max_sample_pages), PROT_READ | PROT_WRITE, MAP_SHARED, prof.fds[0], 0);
  if(prof.metadata == MAP_FAILED) {
    fprintf(stderr, "Failed to mmap room (%zu bytes) for perf samples. Aborting with:\n%s\n", prof.pagesize + (prof.pagesize * max_sample_pages), strerror(errno));
    exit(1);
  }

  /* Initialize */
  ioctl(prof.fds[0], PERF_EVENT_IOC_RESET, 0);
  ioctl(prof.fds[0], PERF_EVENT_IOC_ENABLE, 0);
  prof.consumed = 0;
  prof.total = 0;
  prof.oops = 0;

  printf("Going to profile all every %f seconds.\n", profile_all_rate);
  timer.tv_sec = profile_all_rate;
  timer.tv_nsec = 0;

  while(!sh_should_stop()) {
    get_accesses();
    nanosleep(&timer, NULL);
  }
}

void *profile_one(void *a) {
  int i;
  struct timespec timer;

  for(i = 0; i < num_events; i++) {
    ioctl(prof.fds[i], PERF_EVENT_IOC_RESET, 0);
    ioctl(prof.fds[i], PERF_EVENT_IOC_ENABLE, 0);
  }
  prof.num_intervals = 0;
  prof.running_avg = 0;

  timer.tv_sec = 1;
  timer.tv_nsec = 0;

  while(!sh_should_stop()) {
    get_bandwidth();
    nanosleep(&timer, NULL);
  }
}
