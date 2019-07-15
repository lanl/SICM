#define _LARGEFILE64_SOURCE
#include "sicm_profile.h"
#include "sicm_high.h"
#include "sicm_impl.h"
#include <sys/types.h>
#include <unistd.h>
#include <fcntl.h>

profile_thread prof;
size_t num_rss_samples = 0;
size_t num_acc_samples = 0;
use_tree(double, size_t);
use_tree(size_t, deviceptr);

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

/* Uses libpfm to figure out the event we're going to use */
void sh_get_event() {
  int err;
  size_t i;

  pfm_initialize();
  prof.pfm = malloc(sizeof(pfm_perf_encode_arg_t));

  /* Make sure all of the events work. Initialize the pes. */
  for(i = 0; i < profopts.num_events; i++) {
    memset(prof.pes[i], 0, sizeof(struct perf_event_attr));
    prof.pes[i]->size = sizeof(struct perf_event_attr);
    memset(prof.pfm, 0, sizeof(pfm_perf_encode_arg_t));
    prof.pfm->size = sizeof(pfm_perf_encode_arg_t);
    prof.pfm->attr = prof.pes[i];

    printf("Starting event %s\n", profopts.events[i]);
    printf("%p\n", profopts.events[i]);
    fflush(stdout);

    err = pfm_get_os_event_encoding(profopts.events[i], PFM_PLM2 | PFM_PLM3, PFM_OS_PERF_EVENT, prof.pfm);
    if(err != PFM_SUCCESS) {
      fprintf(stderr, "Failed to initialize event '%s'. Aborting.\n", profopts.events[i]);
      exit(1);
    }

    /* If we're profiling all, set some additional options. */
    if(profopts.should_profile_all) {
      prof.pes[i]->sample_type = PERF_SAMPLE_ADDR;
      prof.pes[i]->sample_period = profopts.sample_freq;
      prof.pes[i]->mmap = 1;
      prof.pes[i]->disabled = 1;
      prof.pes[i]->exclude_kernel = 1;
      prof.pes[i]->exclude_hv = 1;
      prof.pes[i]->precise_ip = 2;
      prof.pes[i]->task = 1;
      prof.pes[i]->sample_period = profopts.sample_freq;
    }
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
  pid_t pid;
  int cpu, group_fd;
  unsigned long flags;

  /* All of this initialization HAS to happen in the main SICM thread.
   * If it's not, the `perf_event_open` system call won't profile
   * the current thread, but instead will only profile the thread that
   * it was run in.
   */

  prof.pagesize = (size_t) sysconf(_SC_PAGESIZE);

  /* Allocate perf structs */
  prof.pes = malloc(sizeof(struct perf_event_attr *) * profopts.num_events);
  prof.fds = malloc(sizeof(int) * profopts.num_events);
  for(i = 0; i < profopts.num_events; i++) {
    prof.pes[i] = malloc(sizeof(struct perf_event_attr));
    prof.fds[i] = 0;
  }

  /* Use libpfm to fill the pe struct */
  if(profopts.should_profile_all || profopts.should_profile_one) {
    sh_get_event();
  }

  /* Open all perf file descriptors, different arguments for each type
   * of profiling.
   */
  if(profopts.should_profile_all) {
    pid = 0;
    cpu = -1;
    group_fd = -1;
    flags = 0;
  } else if(profopts.should_profile_one) {
    pid = -1;
    cpu = 0;
    group_fd = -1;
    flags = 0;
  }
  for(i = 0; i < profopts.num_events; i++) {
    prof.fds[i] = syscall(__NR_perf_event_open, prof.pes[i], pid, cpu, group_fd, flags);
    if(prof.fds[i] == -1) {
      fprintf(stderr, "Error opening perf event %d (0x%llx): %s\n", i, prof.pes[i]->config, strerror(errno));
      exit(1);
    }
  }

  if(profopts.should_profile_rss) {
    prof.pagemap_fd = open("/proc/self/pagemap", O_RDONLY);
    if (prof.pagemap_fd < 0) {
      fprintf(stderr, "Failed to open /proc/self/pagemap. Aborting.\n");
      exit(1);
    }
  }

  /* Start the profiling threads */
  pthread_mutex_init(&prof.mtx, NULL);
  pthread_mutex_lock(&prof.mtx);
  if(profopts.should_profile_all) {
    pthread_create(&prof.profile_all_id, NULL, &profile_all, NULL);
  } else if(profopts.should_profile_one) {
    pthread_create(&prof.profile_one_id, NULL, &profile_one, NULL);
  }
  if(profopts.should_profile_rss) {
#if 0
    pthread_create(&prof.profile_rss_id, NULL, &profile_rss, NULL);
#endif
  }
}

void sh_stop_profile_thread() {
  size_t i, associated;
  int n;

  /* Stop the actual sampling */
  for(i = 0; i < profopts.num_events; i++) {
    ioctl(prof.fds[i], PERF_EVENT_IOC_DISABLE, 0);
  }

  /* Stop the timers and join the threads */
  pthread_mutex_unlock(&prof.mtx);
  if(profopts.should_profile_all) {
    pthread_join(prof.profile_all_id, NULL);
  } else if(profopts.should_profile_one) {
    pthread_join(prof.profile_one_id, NULL);
  }
  if(profopts.should_profile_rss) {
    pthread_join(prof.profile_rss_id, NULL);
  }

  for(i = 0; i < profopts.num_events; i++) {
    close(prof.fds[i]);
  }

  /* PEBS profiling */
  if(profopts.should_profile_all) {
    printf("===== PEBS RESULTS =====\n");
    associated = 0;
    for(i = 0; i <= tracker.max_index; i++) {
      if(!tracker.arenas[i]) continue;
      associated += tracker.arenas[i]->accesses;
      printf("%d sites: ", tracker.arenas[i]->num_alloc_sites);
      for(n = 0; n < tracker.arenas[i]->num_alloc_sites; n++) {
        printf("%d ", tracker.arenas[i]->alloc_sites[n]);
      }
      printf("\n");
      printf("  Accesses: %zu\n", tracker.arenas[i]->accesses);
      if(profopts.should_profile_rss) {
        printf("  Peak RSS: %zu\n", tracker.arenas[i]->peak_rss);
        printf("  Average RSS: %zu\n", tracker.arenas[i]->avg_rss);
      }
    }
    printf("Totals: %zu / %zu\n", associated, prof.total);
    printf("Number of RSS samples: %zu\n", num_rss_samples);
    printf("===== END PEBS RESULTS =====\n");

  /* MBI profiling */
  } else if(profopts.should_profile_one) {
    printf("===== MBI RESULTS FOR SITE %u =====\n", profopts.profile_one_site);
    printf("Average bandwidth: %.1f MB/s\n", prof.running_avg);
    printf("Maximum bandwidth: %.1f MB/s\n", prof.max_bandwidth);
    if(profopts.should_profile_rss) {
      printf("Peak RSS: %zu\n", tracker.arenas[profopts.profile_one_site]->peak_rss);
    }
    printf("===== END MBI RESULTS =====\n");

  /* RSS profiling */
  } else if(profopts.should_profile_rss) {
    printf("===== RSS RESULTS =====\n");
    for(i = 0; i <= tracker.max_index; i++) {
      if(!tracker.arenas[i]) continue;
      printf("Sites: ");
      for(n = 0; n < tracker.arenas[i]->num_alloc_sites; n++) {
        printf("%d ", tracker.arenas[i]->alloc_sites[n]);
      }
      printf("\n");
      if(profopts.should_profile_rss) {
        printf("  Peak RSS: %zu\n", tracker.arenas[i]->peak_rss);
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
  struct sample *sample;
  struct perf_event_header *header;
  int err;
  size_t i;

  num_acc_samples++;
  for(i = 0; i <= tracker.max_index; i++) {
    if(!(tracker.arenas[i])) continue;
    tracker.arenas[i]->cur_accesses = 0;
  }

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
  buf_size = prof.pagesize * profopts.max_sample_pages;
  asm volatile("" ::: "memory"); /* Block after reading data_head, per perf docs */

  base = (char *)prof.metadata + prof.pagesize;
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
      prof.total++;
      /* Search for which extent it goes into */
      extent_arr_for(tracker.extents, i) {
        if(!tracker.extents->arr[i].start && !tracker.extents->arr[i].end) continue;
        arena = tracker.extents->arr[i].arena;
        if((addr >= tracker.extents->arr[i].start) && (addr <= tracker.extents->arr[i].end) && arena) {
          arena->cur_accesses++;
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
  prof.metadata->data_tail = head;
  __sync_synchronize();

  /* Now calculate an average accesses/sample for each arena */
  for(i = 0; i <= tracker.max_index; i++) {
    if(!(tracker.arenas[i])) continue;
    tracker.arenas[i]->accesses += tracker.arenas[i]->cur_accesses;
  }

#if 0
  if(should_profile_online) {
    online_reconfigure();
  }
#endif
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
  prof.num_intervals++;
  prof.running_avg = ((prof.running_avg * (prof.num_intervals - 1)) + total) / prof.num_intervals;

  if(total > prof.max_bandwidth) {
    prof.max_bandwidth = total;
  }
}

#if 0
static void
get_rss() {
	size_t i, n, numpages;
  uint64_t start, end;
  arena_info *arena;

  /* Make sure we don't stomp on any toes */
  pthread_mutex_lock(&arena_lock);
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

    arena->rss += end - start;

		/* Maintain the peak for this arena */
		if(arena->rss > arena->peak_rss) {
			arena->peak_rss = arena->rss;
		}
	}

  /* Maintain a rolling average of the RSS of each arena,
   * using the previous 10 values
   */
  num_rss_samples++;
  for(i = 0; i <= max_index; i++) {
    if(!(arenas[i])) continue;

    if(!(arenas[i]->avg_rss)) {
      arenas[i]->avg_rss = arenas[i]->rss;
    }

    arenas[i]->avg_rss -= arenas[i]->avg_rss / num_rss_samples;
    arenas[i]->avg_rss += arenas[i]->rss / num_rss_samples;
  }

  pthread_rwlock_unlock(&extents_lock);
  pthread_mutex_unlock(&arena_lock);
}
#endif

#if 0
static void
get_rss() {
	size_t i, n, numpages;
  uint64_t start, end;
  arena_info *arena;
  ssize_t num_read;

  /* Grab the lock for the extents array */
  //pthread_mutex_lock(&arena_lock);
  pthread_rwlock_rdlock(&extents_lock);

	/* Zero out the RSS values for each arena */
	extent_arr_for(rss_extents, i) {
    arena = rss_extents->arr[i].arena;
    if(!arena) continue;
		arena->rss = 0;
	}

	/* Iterate over the chunks */
	extent_arr_for(rss_extents, i) {
		start = (uint64_t) rss_extents->arr[i].start;
		end = (uint64_t) rss_extents->arr[i].end;
		arena = rss_extents->arr[i].arena;
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

  num_rss_samples++;
  pthread_rwlock_unlock(&extents_lock);
  //pthread_mutex_unlock(&arena_lock);
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
#endif

void *profile_all(void *a) {
  struct timespec timer;
  size_t i;

  /* mmap the file */
  prof.metadata = mmap(NULL, prof.pagesize + (prof.pagesize * profopts.max_sample_pages), PROT_READ | PROT_WRITE, MAP_SHARED, prof.fds[0], 0);
  if(prof.metadata == MAP_FAILED) {
    fprintf(stderr, "Failed to mmap room (%zu bytes) for perf samples. Aborting with:\n%s\n", prof.pagesize + (prof.pagesize * profopts.max_sample_pages), strerror(errno));
    exit(1);
  }

  /* Initialize */
  for(i = 0; i < profopts.num_events; i++) {
    ioctl(prof.fds[i], PERF_EVENT_IOC_RESET, 0);
    ioctl(prof.fds[i], PERF_EVENT_IOC_ENABLE, 0);
  }
  prof.consumed = 0;
  prof.total = 0;
  prof.oops = 0;

  timer.tv_sec = profopts.profile_all_rate;
  timer.tv_nsec = 0;

  while(!sh_should_stop()) {
    get_accesses();
    nanosleep(&timer, NULL);
  }
}

void *profile_one(void *a) {
  int i;
  struct timespec timer;

  for(i = 0; i < profopts.num_events; i++) {
    ioctl(prof.fds[i], PERF_EVENT_IOC_RESET, 0);
    ioctl(prof.fds[i], PERF_EVENT_IOC_ENABLE, 0);
  }
  prof.num_intervals = 0;
  prof.running_avg = 0;
  prof.max_bandwidth = 0;

  timer.tv_sec = 1;
  timer.tv_nsec = 0;

  while(!sh_should_stop()) {
    get_bandwidth();
    nanosleep(&timer, NULL);
  }
}
