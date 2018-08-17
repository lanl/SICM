#include "high.h"
#include "profile.h"
#include "sicmimpl.h"
#include "rbtree.h"

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
  prof.pe->sample_period = 512;

  /* Generic options */
  prof.pe->disabled = 1;
  prof.pe->exclude_kernel = 1;
  prof.pe->exclude_hv = 1;
  prof.pe->precise_ip = 2;
  prof.pe->mmap = 1;
  prof.pe->task = 1;
  prof.pe->use_clockid = 1;
  prof.pe->clockid = CLOCK_MONOTONIC_RAW;

  /* Open the perf file descriptor */
  prof.fd = syscall(__NR_perf_event_open, prof.pe, 0, -1, -1, 0);
  if (prof.fd == -1) {
    fprintf(stderr, "Error opening leader %llx\n", prof.pe->config);
    exit(EXIT_FAILURE);
  }

  /* mmap the file */
  prof.page_size = (size_t) sysconf(_SC_PAGESIZE);
  prof.metadata = mmap(NULL, prof.page_size + (prof.page_size * 64), PROT_READ, MAP_SHARED, prof.fd, 0);

  /* Start the sampling */
  ioctl(prof.fd, PERF_EVENT_IOC_RESET, 0);
  ioctl(prof.fd, PERF_EVENT_IOC_ENABLE, 0);

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

void *sh_profile_thread(void *a) {
  uint64_t consumed, head;
  struct perf_event_header *header;
  struct sample *sample;
  unsigned long long count, associated;
  size_t i;
  arena_info *arena;
  void *addr;
  
  consumed = 0;
  count = 0;
  header = (struct perf_event_header *)((char *)prof.metadata + prof.page_size);
  while(!sh_should_stop()) {
    /* Get ready to read */
    head = prof.metadata->data_head;

    /* Read all of the samples */
    while(consumed < head) {
      if(header->size == 0) {
        break;
      }
      sample = (struct sample *)((char *)(header) + 8);
      addr = (void *)sample->addr;
      if(addr) {
        count++;

        /* Search for which extent it goes into */
        extent_arr_for(extents, i) {
          if(!extents->arr[i].start && !extents->arr[i].end) continue;
          arena = extents->arr[i].arena;
          if((addr >= extents->arr[i].start) && (addr <= extents->arr[i].end)) {
            arena->accesses++;
          }
        }
      }
      consumed = head;
      header = (struct perf_event_header *)((char *)header + header->size);
    }
  }

  associated = 0;
  for(i = 0; i <= max_index; i++) {
    if(!arenas[i]) continue;
    associated += arenas[i]->accesses;
    printf("%u: %llu\n", arenas[i]->id, arenas[i]->accesses);
  }
  printf("Totals: %llu / %llu\n", associated, count);

  printf("Cleaning up thread.\n");
  return NULL;
}
