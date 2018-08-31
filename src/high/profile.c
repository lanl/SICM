#include "high.h"
#include "profile.h"
#include "sicmimpl.h"
#include <unistd.h>
#include <fcntl.h>

profile_thread prof;

void check_error(int err) {
  switch(err) {
    case PFM_ERR_TOOSMALL:
      printf("The code argument is too small for the encoding. \n");
      break;
    case PFM_ERR_INVAL:
      printf("The code or count argument is NULL. \n");
      break;
    case PFM_ERR_NOMEM:
      printf("Not enough memory. \n");
      break;
    case PFM_ERR_NOTFOUND:
      printf("Event not found. \n");
      break;
    case PFM_ERR_ATTR:
      printf("Invalid event attribute (unit mask or modifier) \n");
      break;
    case PFM_ERR_ATTR_VAL:
      printf("Invalid modifier value. \n");
      break;
    case PFM_ERR_ATTR_SET:
      printf("attribute already set, cannot be changed. \n");
      break;
    default:
      printf("Other error.\n");
  };
}

void sh_start_profile_thread() {
  int err;

  /* Initialize the pe struct */
  prof.pe = malloc(sizeof(struct perf_event_attr));
  memset(prof.pe, 0, sizeof(struct perf_event_attr));
  prof.pe->size = sizeof(struct perf_event_attr);

  /* Use libpfm to detect the event that we're going to use */
  pfm_initialize();
  prof.pfm = malloc(sizeof(pfm_perf_encode_arg_t));
  memset(prof.pfm, 0, sizeof(pfm_perf_encode_arg_t));
  prof.pfm->size = sizeof(pfm_perf_encode_arg_t);
  prof.pfm->attr = prof.pe;
  err = pfm_get_os_event_encoding("MEM_LOAD_UOPS_RETIRED.L3_MISS", PFM_PLM2 | PFM_PLM3, PFM_OS_PERF_EVENT, prof.pfm);
  if(err != PFM_SUCCESS) {
    check_error(err);
    exit(1);
  }
  printf("%llx\n", prof.pe->config);

  /* Make sure we grab PEBS addresses */
  prof.pe->sample_type = PERF_SAMPLE_ADDR;
  prof.pe->sample_period = sample_freq;

  /* Generic options */
  prof.pe->disabled = 1;
  prof.pe->exclude_kernel = 1;
  prof.pe->exclude_hv = 1;
  prof.pe->precise_ip = 2;
  prof.pe->mmap = 1;
  prof.pe->task = 1;
  /*
  prof.pe->use_clockid = 1;
  prof.pe->clockid = CLOCK_MONOTONIC_RAW;
  */

  /* Open the perf file descriptor */
  prof.fd = syscall(__NR_perf_event_open, prof.pe, 0, -1, -1, 0);
  if (prof.fd == -1) {
    fprintf(stderr, "Error opening leader %llx\n", prof.pe->config);
    exit(EXIT_FAILURE);
  }

  /* mmap the file */
  prof.pagesize = (size_t) sysconf(_SC_PAGESIZE);
  printf("Allocating %zu\n", prof.pagesize + (prof.pagesize * max_sample_pages));
  prof.metadata = mmap(NULL, prof.pagesize + (prof.pagesize * max_sample_pages), PROT_READ | PROT_WRITE, MAP_SHARED, prof.fd, 0);
  if(prof.metadata == MAP_FAILED) {
    fprintf(stderr, "Failed to mmap room for perf samples. Aborting with:\n%s\n", strerror(errno));
    exit(1);
  }

  /* Start the sampling */
  ioctl(prof.fd, PERF_EVENT_IOC_RESET, 0);
  ioctl(prof.fd, PERF_EVENT_IOC_ENABLE, 0);

  /* Initialize for get_accesses */
  prof.consumed = 0;
  prof.total = 0;
  //prof.header = (struct perf_event_header *)((char *)prof.metadata + prof.pagesize);

  /* Initialize for get_rss */
  prof.pagemap_fd = open("/proc/self/pagemap", O_RDONLY);
  if (prof.pagemap_fd < 0) {
    fprintf(stderr, "Failed to open /proc/self/pagemap. Aborting.\n");
    exit(1);
  }
  prof.pfndata = NULL;
  prof.addrsize = sizeof(uint64_t);

  /* Start the profiling thread */
  pthread_mutex_init(&prof.mtx, NULL);
  pthread_mutex_lock(&prof.mtx);
  pthread_create(&prof.id, NULL, &sh_profile_thread, NULL);
}

void sh_stop_profile_thread() {
	/* Stop the actual sampling */
	ioctl(prof.fd, PERF_EVENT_IOC_DISABLE, 0);
  close(prof.fd);

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
  prof.pfd.fd = prof.fd;
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
  printf("Setting data_tail to %p\n", (char *) head);
  prof.metadata->data_tail = head;
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

  /* Loop until we're stopped by the destructor */
  while(!sh_should_stop()) {

    /* Use PEBS to get the accesses to each arena */
    get_accesses();

    /* Gather the RSS */
    get_rss();
  }

  /* Print out the results of the profiling */
  associated = 0;
  for(i = 0; i <= max_index; i++) {
    if(!arenas[i]) continue;
    associated += arenas[i]->accesses;
    printf("%u:\n", arenas[i]->id);
    printf("  Accesses: %zu\n", arenas[i]->accesses);
    printf("  Peak RSS: %zu\n", arenas[i]->peak_rss);
  }
  printf("Totals: %zu / %zu\n", associated, prof.total);

  printf("Cleaning up thread.\n");
  return NULL;
}
