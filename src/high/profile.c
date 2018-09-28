#include "sicm_high.h"
#include "sicm_profile.h"
#include "sicm_impl.h"
#include <unistd.h>
#include <fcntl.h>

profile_thread prof;

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
    buf = calloc(max_imc_len + 1, sizeof(char));
    for(i = 1; i < num_imcs; i++) {

      /* Initialize the pe struct */
      memset(prof.pes[i], 0, sizeof(struct perf_event_attr));
      prof.pes[i]->size = sizeof(struct perf_event_attr);
      memset(prof.pfm, 0, sizeof(pfm_perf_encode_arg_t));
      prof.pfm->size = sizeof(pfm_perf_encode_arg_t);
      prof.pfm->attr = prof.pes[i];

      /* Construct the event string from the IMC and the event */
      memset(buf, 0, sizeof(char) * (max_imc_len + 1));
      sprintf(buf, "%s::%s", imcs[i], *event);

      err = pfm_get_os_event_encoding(buf, PFM_PLM2 | PFM_PLM3, PFM_OS_PERF_EVENT, prof.pfm);
      if(err != PFM_SUCCESS) {
        fprintf(stderr, "Failed to use libpfm with event string %s. Aborting.\n", buf);
        exit(1);
      }
    }
    
  }
}

void sh_start_profile_thread() {
  int i, group_fd;

  /* Set the pagesize for the RSS and PEBS threads */
  printf("About to get the page size.\n");
  if(should_profile_rss || should_profile_all) {
    prof.pagesize = (size_t) sysconf(_SC_PAGESIZE);
    printf("The page size is %d.\n", prof.pagesize);
  }

  /* Figure out how many events we're going to poll */
  num_events = 0;
  if(should_profile_all) {
    num_events = 1;
  } else if(should_profile_one) {
    num_events = num_imcs;
  }

  /* Initialize the pe structs */
  prof.pes = malloc(sizeof(struct perf_event_attr *) * num_events);
  prof.fds = malloc(sizeof(int) * num_events);
  for(i = 0; i < num_events; i++) {
    prof.pes[i] = malloc(sizeof(struct perf_event_attr));
    prof.fds[i] = 0;
  }

  /* Fill in the specifics of the pe structs with libpfm */
  sh_get_event();

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

  if(should_profile_all) {
    /* mmap the file */
    prof.metadata = mmap(NULL, prof.pagesize + (prof.pagesize * max_sample_pages), PROT_READ | PROT_WRITE, MAP_SHARED, prof.fds[0], 0);
    if(prof.metadata == MAP_FAILED) {
      fprintf(stderr, "Failed to mmap room (%zu bytes) for perf samples. Aborting with:\n%s\n", prof.pagesize + (prof.pagesize * max_sample_pages), strerror(errno));
      exit(1);
    }
  }

  /* Start the sampling */
  for(i = 0; i < num_events; i++) {
    ioctl(prof.fds[i], PERF_EVENT_IOC_RESET, 0);
    ioctl(prof.fds[i], PERF_EVENT_IOC_ENABLE, 0);
  }

  /* Initialize for get_accesses */
  if(should_profile_all) {
    prof.consumed = 0;
    prof.total = 0;
  }

  /* Initialize for get_rss */
  if(should_profile_rss) {
    prof.pagemap_fd = open("/proc/self/pagemap", O_RDONLY);
    if (prof.pagemap_fd < 0) {
      fprintf(stderr, "Failed to open /proc/self/pagemap. Aborting.\n");
      exit(1);
    }
    prof.pfndata = NULL;
    prof.addrsize = sizeof(uint64_t);
  }

  /* Start the profiling thread */
  pthread_mutex_init(&prof.mtx, NULL);
  pthread_mutex_lock(&prof.mtx);
  pthread_create(&prof.id, NULL, &sh_profile_thread, NULL);
}

void sh_stop_profile_thread() {
  int i;

	/* Stop the actual sampling */
  for(i = 0; i < num_events; i++) {
    ioctl(prof.fds[i], PERF_EVENT_IOC_DISABLE, 0);
    close(prof.fds[i]);
  }

  /* Signal the profiling thread to stop */
  pthread_mutex_unlock(&prof.mtx);
  pthread_join(prof.id, NULL);
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

/* Adds up accesses to the arenas */
static inline void get_accesses() {
  uint64_t head, tail, buf_size;
  arena_info *arena;
  void *addr;
  char *base, *begin, *end;
  size_t i;
  struct sample *sample;
  struct perf_event_header *header;

  /* Wait for the perf buffer to be ready */
  prof.pfd.fd = prof.fds[0];
  prof.pfd.events = POLLIN;
  prof.pfd.revents = 0;
  poll(&prof.pfd, 1, 1000);

  /* Get ready to read */
  head = prof.metadata->data_head;
  tail = prof.metadata->data_tail;
  buf_size = prof.pagesize * max_sample_pages;
  asm volatile("" ::: "memory"); /* Block after reading data_head, per perf docs */

  base = (char *)prof.metadata  + prof.pagesize;
  begin = base + tail % buf_size;
  end = base + head % buf_size;

  /* Read all of the samples */
  while(begin != end) {

    header = (struct perf_event_header *)begin;
    if(header->size == 0) {
      break;
    }
    sample = (struct sample *) (begin + 8);
    addr = (void *) sample->addr;

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

  /* Let perf know that we've read this far */
  __sync_synchronize();
  prof.metadata->data_tail = head;
}

static void
get_bandwidth(int sig, siginfo_t *si, void *uc)
{
  float count_f;
  long long count;
  int num, i;
  struct itimerspec it;

  /* Stop the counter and read the value if it has been at least a second */
  printf("=========\n");
  for(i = 0; i < num_events; i++) {
    ioctl(prof.fds[i], PERF_EVENT_IOC_DISABLE, 0);
    read(prof.fds[i], &count, sizeof(long long));
    count_f = (float) count;
    printf("%d: %.1f MB/s\n", i, count_f * 64 / 1024 / 1024);

    /* Start it back up again */
    ioctl(prof.fds[i], PERF_EVENT_IOC_RESET, 0);
    ioctl(prof.fds[i], PERF_EVENT_IOC_ENABLE, 0);
  }
  printf("=========\n");
}

static inline void get_rss() {
	size_t i, n, numpages;
  uint64_t start, end;
  arena_info *arena;

	/* Zero out the RSS values for each arena */
	extent_arr_for(extents, i) {
    arena = extents->arr[i].arena;
		arena->rss = 0;
	}

	/* Iterate over the chunks */
	extent_arr_for(extents, i) {

		start = (uint64_t) extents->arr[i].start;
		end = (uint64_t) extents->arr[i].end;
		arena = extents->arr[i].arena;

		numpages = (end - start) / prof.pagesize;
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
}

void *sh_profile_thread(void *a) {
  size_t i, associated;
  struct itimerspec its;
  struct sigaction sa;

  if(should_profile_one) {
    /* Signal handler for bandwidth */
	  sa.sa_flags = SA_SIGINFO;
	  sa.sa_sigaction = get_bandwidth;
	  sigemptyset(&sa.sa_mask);
	  if (sigaction(SIGRTMIN, &sa, NULL) == -1) {
      fprintf(stderr, "Failed to create a signal. Aborting.\n");
      exit(1);
    }

    /* Create a timer for the bandwdith counting */
    prof.sev.sigev_notify = SIGEV_SIGNAL;
		prof.sev.sigev_signo = SIGRTMIN;
    prof.sev.sigev_value.sival_ptr = &prof.timerid;
    if(timer_create(CLOCK_REALTIME, &prof.sev, &prof.timerid) == -1) {
      fprintf(stderr, "Failed to create a timer. Aborting.\n");
      exit(1);
    }

    /* Arm the timer for 1 second */
    its.it_value.tv_sec = 1;
    its.it_value.tv_nsec = 0;
    its.it_interval.tv_sec = 1;
    its.it_interval.tv_nsec = 0;
    if(timer_settime(prof.timerid, 0, &its, NULL) == -1) {
      fprintf(stderr, "Failed to set the timer. Aborting.\n");
      exit(1);
    }
  }

  /* Loop until we're stopped by the destructor */
  while(!sh_should_stop()) {

    /* Use PEBS to get the accesses to each arena */
    if(should_profile_all) {
      get_accesses();
    }

    /* Gather the RSS */
    if(should_profile_rss) {
      get_rss();
    }
  }

  /* Print out the results of the profiling */
  if(should_profile_one) {
  }
  if(should_profile_all) {
    associated = 0;
    for(i = 0; i <= max_index; i++) {
      if(!arenas[i]) continue;
      associated += arenas[i]->accesses;
      printf("%u:\n", arenas[i]->id);
      printf("  Accesses: %zu\n", arenas[i]->accesses);
      printf("  Peak RSS: %zu\n", arenas[i]->peak_rss);
    }
    printf("Totals: %zu / %zu\n", associated, prof.total);
  }

  if(should_profile_one) {
    timer_delete(prof.timerid);
  }

  return NULL;
}
