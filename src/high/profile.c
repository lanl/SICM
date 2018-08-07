#include "profile.h"

profile_thread prof;

void sh_start_profile_thread() {
  int err;

  prof.page_size = (size_t) sysconf(_SC_PAGESIZE);

  /* Use libpfm to detect the event that we're going to use */
  memset(&prof.pe, 0, sizeof(struct perf_event_attr));
  prof.pe.size = sizeof(struct perf_event_attr);
  pfm_initialize();
  memset(&prof.pfm, 0, sizeof(pfm_perf_encode_arg_t));
  prof.pfm.size = sizeof(pfm_perf_encode_arg_t);
  prof.pfm.attr = &prof.pe;
  err = pfm_get_os_event_encoding("MEM_UOPS_RETIRED:L3_MISS", PFM_PLM0 | PFM_PLM3, PFM_OS_PERF_EVENT, &prof.pfm);
  if(err != PFM_SUCCESS) {
    printf("FAILURE\n");
    exit(1);
  }

	/* Make sure we grab PEBS addresses */
  prof.pe.sample_type = PERF_SAMPLE_ADDR;
  prof.pe.sample_period = 1000;

  /* Generic options */
  prof.pe.disabled = 1;
  prof.pe.exclude_kernel = 1;
  prof.pe.exclude_hv = 1;
  prof.pe.precise_ip = 2;
  prof.pe.mmap = 1;
  prof.pe.task = 1;
  prof.pe.use_clockid = 1;
  prof.pe.clockid = CLOCK_MONOTONIC_RAW;

  /* Open the perf file descriptor */
  prof.fd = syscall(__NR_perf_event_open, &prof.pe, 0, -1, -1, 0);
  if (prof.fd == -1) {
    fprintf(stderr, "Error opening leader %llx\n", prof.pe.config);
    exit(EXIT_FAILURE);
  }

	/* mmap the file */
  prof.metadata = mmap(NULL, prof.page_size + (prof.page_size * 64), PROT_WRITE, MAP_SHARED, prof.fd, 0);

  /* Start the profiling thread */
  pthread_mutex_init(&prof.mtx, NULL);
  pthread_mutex_lock(&prof.mtx);
  pthread_create(&prof.id, NULL, &sh_profile_thread, NULL);
}

void sh_stop_profile_thread() {
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

void *sh_profile_thread(void *args) {
  while(!sh_should_stop()) {
    printf("Running thread.\n");
  }
  return NULL;
}
