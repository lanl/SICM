#include <sys/types.h>
#include <unistd.h>
#include <fcntl.h>
#include <time.h>
#include <sys/syscall.h>
#include <signal.h>
#include "sicm_high.h"

profiler prof;
use_tree(double, size_t);
use_tree(size_t, deviceptr);

void setup_timer(profile_thread *pt) {
  sigset_t mask;

  /* We need to wait until the thread has initialized
   * its tid */
  while(pt->tid == NULL) {
  }

  /* Set up the signal handler */
  pt->sa.sa_flags = SA_SIGINFO;
  pt->sa.sa_handler = pt->func;
  sigemptyset(&pt->sa.sa_mask);
  if(sigaction(SIGRTMIN, &pt->sa, NULL) == -1) {
    fprintf(stderr, "Error creating signal handler. Aborting.\n");
    exit(1);
  }

  /* Block the signal for a bit */
  sigemptyset(&mask);
  sigaddset(&mask, SIGRTMIN);
  if(sigprocmask(SIG_SETMASK, &mask, NULL) == -1) {
    fprintf(stderr, "Error blocking signal. Aborting.\n");
    exit(1);
  }

  /* Create the timer */
  pt->sev.sigev_notify = SIGEV_THREAD_ID;
  pt->sev.sigev_signo = SIGRTMIN;
  pt->sev.sigev_value.sival_ptr = &pt->timer;
  pt->sev._sigev_un._tid = *pt->tid;
  if(timer_create(CLOCK_REALTIME, &pt->sev, &pt->timer) == -1) {
    fprintf(stderr, "Error creating timer. Aborting.\n");
    exit(1);
  }
  
  /* Set the timer */
  pt->its.it_value.tv_sec = profopts.profile_rate_nseconds / 1000000000;
  pt->its.it_value.tv_nsec = profopts.profile_rate_nseconds % 1000000000;
  pt->its.it_interval.tv_sec = pt->its.it_value.tv_sec;
  pt->its.it_interval.tv_nsec = pt->its.it_value.tv_nsec;
  if(timer_settime(pt->timer, 0, &pt->its, NULL) == -1) {
    fprintf(stderr, "Error setting the timer. Aborting.\n");
    exit(1);
  }

  /* Unblock the signal */
  if(sigprocmask(SIG_UNBLOCK, &mask, NULL) == -1) {
    fprintf(stderr, "Error unblocking signal. Aborting.\n");
    exit(1);
  }
}

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
    prof.pfndata = NULL;
    prof.addrsize = sizeof(uint64_t);
    prof.pagesize = (size_t) sysconf(_SC_PAGESIZE);
  }

  /* Start the profiling threads */
  pthread_mutex_init(&prof.mtx, NULL);
  pthread_mutex_lock(&prof.mtx);
  if(profopts.should_profile_all) {
    prof.profile_all.tid = NULL;
    prof.profile_all.func = &get_accesses;
    pthread_create(&prof.profile_all.id, NULL, &profile_all, NULL);
    setup_timer(&prof.profile_all);
  }
  if(profopts.should_profile_one) {
    prof.profile_one.tid = NULL;
    prof.profile_one.func = &get_bandwidth;
    pthread_create(&prof.profile_one.id, NULL, &profile_one, NULL);
    setup_timer(&prof.profile_one);
  }
  if(profopts.should_profile_rss) {
    prof.profile_rss.tid = NULL;
    prof.profile_rss.func = &get_rss;
    pthread_create(&prof.profile_rss.id, NULL, &profile_rss, NULL);
    setup_timer(&prof.profile_rss);
  }
  if(profopts.should_profile_allocs) {
    prof.profile_allocs.tid = NULL;
    prof.profile_allocs.func = &get_allocs;
    pthread_create(&prof.profile_allocs.id, NULL, &profile_allocs, NULL);
    setup_timer(&prof.profile_allocs);
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

void sh_stop_profile_thread() {
  size_t i, n, x;

  /* Stop the actual sampling */
  for(i = 0; i < profopts.num_events; i++) {
    ioctl(prof.fds[i], PERF_EVENT_IOC_DISABLE, 0);
  }

  /* Stop the timers and join the threads */
  pthread_mutex_unlock(&prof.mtx);
  if(profopts.should_profile_all) {
    pthread_join(prof.profile_all.id, NULL);
  }
  if(profopts.should_profile_one) {
    pthread_join(prof.profile_one.id, NULL);
  }
  if(profopts.should_profile_rss) {
    pthread_join(prof.profile_rss.id, NULL);
  }
  if(profopts.should_profile_allocs) {
    pthread_join(prof.profile_allocs.id, NULL);
  }

  for(i = 0; i < profopts.num_events; i++) {
    close(prof.fds[i]);
  }

  /* PEBS profiling */
  if(profopts.should_profile_all) {
    printf("===== PEBS RESULTS =====\n");
    for(i = 0; i <= tracker.max_index; i++) {
      if(!tracker.arenas[i]) continue;

      /* Print the sites that are in this arena */
      printf("%d sites: ", tracker.arenas[i]->num_alloc_sites);
      for(n = 0; n < tracker.arenas[i]->num_alloc_sites; n++) {
        printf("%d ", tracker.arenas[i]->alloc_sites[n]);
      }
      printf("\n");

      /* Print the RSS of the arena */
      if(profopts.should_profile_rss) {
        printf("  Peak RSS: %zu\n", tracker.arenas[i]->peak_rss);
      }
      printf("    Number of intervals: %zu\n", tracker.arenas[i]->num_intervals);
      printf("    First interval: %zu\n", tracker.arenas[i]->first_interval);

      /* Print information for each event */
      for(n = 0; n < profopts.num_events; n++) {
        printf("  Event: %s\n", profopts.events[n]);
        printf("    Total: %zu\n", tracker.arenas[i]->profiles[n].total);
        for(x = 0; x < tracker.arenas[i]->num_intervals; x++) {
          printf("      %zu\n", tracker.arenas[i]->profiles[n].interval_vals[x]);
        }
      }
    }
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

void *profile_rss(void *a) {
  struct timespec timer;
  pid_t *tid;

  /* Defined the moment the pointer is non-NULL */
  tid = malloc(sizeof(pid_t));
  *tid = syscall(SYS_gettid);
  prof.profile_rss.tid = tid;

  while(!sh_should_stop()) {
  }
}

void *profile_all(void *a) {
  size_t i;
  pid_t *tid;

  /* mmap the file */
  prof.metadata = malloc(sizeof(struct perf_event_mmap_page *) * profopts.num_events);
  for(i = 0; i < profopts.num_events; i++) {
    prof.metadata[i] = mmap(NULL, prof.pagesize + (prof.pagesize * profopts.max_sample_pages), PROT_READ | PROT_WRITE, MAP_SHARED, prof.fds[i], 0);
    if(prof.metadata[i] == MAP_FAILED) {
      fprintf(stderr, "Failed to mmap room (%zu bytes) for perf samples. Aborting with:\n%s\n", prof.pagesize + (prof.pagesize * profopts.max_sample_pages), strerror(errno));
      exit(1);
    }
  }

  /* Initialize */
  for(i = 0; i < profopts.num_events; i++) {
    ioctl(prof.fds[i], PERF_EVENT_IOC_RESET, 0);
    ioctl(prof.fds[i], PERF_EVENT_IOC_ENABLE, 0);
  }

  /* Defined the moment the pointer is non-NULL */
  tid = malloc(sizeof(pid_t));
  *tid = syscall(SYS_gettid);
  prof.profile_all.tid = tid;

  while(!sh_should_stop()) {
    /*
    get_accesses();
    */
  }
}

void *profile_one(void *a) {
  int i;

  for(i = 0; i < profopts.num_events; i++) {
    ioctl(prof.fds[i], PERF_EVENT_IOC_RESET, 0);
    ioctl(prof.fds[i], PERF_EVENT_IOC_ENABLE, 0);
  }
  prof.num_bandwidth_intervals = 0;
  prof.running_avg = 0;
  prof.max_bandwidth = 0;

  while(!sh_should_stop()) {
    /*
    get_bandwidth();
    */
  }
}

void *profile_allocs(void *a) {
  while(!sh_should_stop()) {
  }
}
