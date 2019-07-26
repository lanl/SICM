#include <sys/types.h>
#include <unistd.h>
#include <fcntl.h>
#include <time.h>
#include <sys/syscall.h>
#include <signal.h>
#include "sicm_high.h"

profiler prof;
static int global_signal;

/* Allocates room for profiling information for this arena.
 * Returns a pointer to the profile_info struct as a void pointer
 * so that the arena can have a pointer to its profiling information.
 * Some profilers use this pointer to get to the profiling information
 * when all they have is a pointer to the arena.
 */
void *create_profile_arena(int index) {
  prof.info[index] = calloc(1, sizeof(profile_info));
  prof.info[index]->num_intervals = 0;
  prof.info[index]->first_interval = 0;

  if(profopts.should_profile_all) {
    profile_all_arena_init(&(prof.info[index]->profile_all));
  }
#if 0
  if(profopts.should_profile_rss) {
    profile_rss_arena_init(&(prof.info[index]->profile_rss));
  }
  if(profopts.should_profile_one) {
    profile_one_arena_init(&(prof.info[index]->profile_one));
  }
  if(profopts.should_profile_allocs) {
    profile_allocs_arena_init(&(prof.info[index]->profile_allocs));
  }
#endif

  /* Return this so that the arena can have a pointer to its profiling
   * information
   */
  return (void *)prof.info[index];
}

/* Function used by the profile threads to block/unblock
 * their own signal.
 */
void block_signal(int signal) {
	sigset_t mask;
  struct timeval tv;

  /* Block the signal */
  sigemptyset(&mask);
  sigaddset(&mask, signal);
  if(sigprocmask(SIG_SETMASK, &mask, NULL) == -1) {
    fprintf(stderr, "Error blocking signal %d. Aborting.\n", signal);
    exit(1);
  }

  /* Print out what time we were triggered */
  syscall(SYS_gettimeofday, &tv, NULL);
  printf("Profiling thread %d triggered: %ld.%06ld\n", signal, tv.tv_sec, tv.tv_usec);
  fflush(stdout);
}

/* Unblocks a signal. Also notifies the Master thread. */
void unblock_signal(int signal) {
	sigset_t mask;
  struct timeval tv;

  /* Signal the master thread that we're done */
  pthread_mutex_lock(&prof.mtx);
  syscall(SYS_gettimeofday, &tv, NULL);
  printf("Profiling thread %d done: %ld.%06ld\n", signal, tv.tv_sec, tv.tv_usec);
  fflush(stdout);
  prof.threads_finished++;
  pthread_cond_signal(&prof.cond);
  pthread_mutex_unlock(&prof.mtx);

  sigemptyset(&mask);
  sigaddset(&mask, signal);
  if(sigprocmask(SIG_UNBLOCK, &mask, NULL) == -1) {
    fprintf(stderr, "Error unblocking signal. Aborting.\n");
    exit(1);
  }
}

/* This is the signal handler for the Master thread, so
 * it does this on every interval.
 */
void profile_master_interval(int s) {
	struct timeval tv;
  size_t i;
  unsigned copy;
  syscall(SYS_gettimeofday, &tv, NULL);

  printf("\n\n====================\n");
  printf("Master triggered at time %ld.%06ld\n", tv.tv_sec, tv.tv_usec);
  fflush(stdout);

  for(i = 0; i <= tracker.max_index; i++) {
    profinfo = prof.info[i];
    if(!profinfo) continue;
    if(profinfo->num_intervals == 0) {
      /* This is the arena's first interval, make note */
      profinfo->first_interval = prof.cur_interval;
    }
    profinfo->num_intervals++;
  }

  /* Notify the threads */
  for(i = 0; i < prof.num_profile_threads; i++) {
    pthread_kill(prof.profile_threads[i].id, prof.profile_threads[i].signal);
  }

  /* Wait for the threads to do their bit */
  pthread_mutex_lock(&prof.mtx);
  while(1) {
    if(prof.threads_finished) {
      /* At least one thread is finished, check if it's all of them */
      copy = prof.threads_finished;
      pthread_mutex_unlock(&prof.mtx);
      if(prof.threads_finished == 2) {
        /* They're all done. */
        printf("Profiling threads are all done.\n");
        fflush(stdout);
        pthread_mutex_lock(&prof.mtx);
        prof.threads_finished = 0;
        break;
      }
      /* At least one was done, but not all of them. Continue waiting. */
      pthread_mutex_lock(&prof.mtx);
    } else {
      /* Wait for at least one thread to signal us */
      pthread_cond_wait(&prof.cond, &prof.mtx);
    }
  }

  /* Finished handling this interval. Wait for another. */
  prof.cur_interval++;
  pthread_mutex_unlock(&prof.mtx);
  printf("Exited the loop, we're now ready to receive another signal\n");
  printf("====================\n");
  fflush(stdout);
}

void setup_profile_thread(void *(*main)(void *), /* Spinning loop function */
                          void (*interval)(int), /* Per-interval function */
                          unsigned long skip_intervals) {
  struct sigaction sa;
  profile_thread *profthread;

  /* Add a new profile_thread struct for it */
  prof.num_profile_threads++;
  prof.profile_threads = realloc(prof.profile_threads, sizeof(profile_thread) * prof.num_profile_threads);
  profthread = &(prof.profile_threads[prof.num_profile_threads - 1]);

  /* Start the thread */
  pthread_create(&(profthread->id), NULL, &main, NULL);

  /* Set up the signal handler */
  profthread->signal = global_signal;
	sa.sa_flags = 0;
  sa.sa_handler = interval;
  sigemptyset(&sa.sa_mask);
  if(sigaction(profthread->signal, &sa, NULL) == -1) {
    fprintf(stderr, "Error creating slave signal handler. Aborting.\n");
    exit(1);
  }

  profthread->skip_intervals = skip_intervals;

  /* Get ready for the next one */
  global_signal++;
}

/* This is the Master thread, it keeps track of intervals
 * and starts/stops the profiling threads. It has a timer
 * which signals it at a certain interval. Each time this
 * happens, it notifies the profiling threads.
 */
void *profile_master(void *a) {
  timer_t timerid;
  struct sigevent sev;
  struct sigaction sa;
  struct itimerspec its;
  long long frequency;
  sigset_t mask;
  pid_t tid;
  int master_signal;

  if(profopts.should_profile_all) {
    setup_profile_thread(&profile_all, &profile_all_interval, 0);
  }
#if 0
  if(profopts.should_profile_rss) {
    setup_profile_thread(&profile_rss, &profile_rss_interval, 0);
  }
  if(profopts.should_profile_one) {
    setup_profile_thread(&profile_one, &profile_one_interval, 0);
  }
  if(profopts.should_profile_allocs) {
    setup_profile_thread(&profile_allocs, &profile_allocs_interval, 0);
  }
#endif
  
  /* Initialize synchronization primitives */
  pthread_mutex_init(&prof.mtx, NULL);
  pthread_cond_init(&prof.cond, NULL);
  prof.cur_interval = 0;
  global_signal = SIGRTMIN;

  /* Set up a signal handler for the master */
  sa.sa_flags = 0;
  sa.sa_handler = profile_master_interval;
  sigemptyset(&sa.sa_mask);
  if(sigaction(master_signal, &sa, NULL) == -1) {
    fprintf(stderr, "Error creating signal handler. Aborting.\n");
    exit(1);
  }

  /* Block the signal for a bit */
  sigemptyset(&mask);
  sigaddset(&mask, master_signal);
  if(sigprocmask(SIG_SETMASK, &mask, NULL) == -1) {
    fprintf(stderr, "Error blocking signal. Aborting.\n");
    exit(1);
  }

  /* Create the timer */
  tid = syscall(SYS_gettid);
  sev.sigev_notify = SIGEV_THREAD_ID;
  sev.sigev_signo = master_signal;
  sev.sigev_value.sival_ptr = &timerid;
  sev._sigev_un._tid = tid;
  if(timer_create(CLOCK_REALTIME, &sev, &timerid) == -1) {
    fprintf(stderr, "Error creating timer. Aborting.\n");
    exit(1);
  }
  
  /* Set the timer */
  its.it_value.tv_sec = profopts.profile_rate_nseconds / 1000000000;
  its.it_value.tv_nsec = profopts.profile_rate_nseconds % 1000000000;
  its.it_interval.tv_sec = its.it_value.tv_sec;
  its.it_interval.tv_nsec = its.it_value.tv_nsec;
  if(timer_settime(timerid, 0, &its, NULL) == -1) {
    fprintf(stderr, "Error setting the timer. Aborting.\n");
    exit(1);
  }

  /* Unblock the signal */
  if(sigprocmask(SIG_UNBLOCK, &mask, NULL) == -1) {
    fprintf(stderr, "Error unblocking signal. Aborting.\n");
    exit(1);
  }

  /* Now we wait for the timer to signal us */
  /* TODO: implement how to stop the master thread */
  while(1) {}
}

void initialize_profiling() {
  /* Allocate room for the per-arena profiling information */
  prof.info = calloc(tracker.max_arenas, sizeof(profile_info *));

  /* All of this initialization HAS to happen in the main SICM thread.
   * If it's not, the `perf_event_open` system call won't profile
   * the current thread, but instead will only profile the thread that
   * it was run in.
   */
  if(profopts.should_profile_all) {
    profile_all_init();
  }
#if 0
  if(profopts.should_profile_rss) {
    profile_rss_init();
  }
  if(profopts.should_profile_one) {
    profile_one_init();
  }
  if(profopts.should_profile_allocs) {
    profile_allocs_init();
  }
#endif
}

void sh_start_profile_master_thread() {

  /* This initializes the values that the threads will need to do their profiling,
   * including perf events, file descriptors, etc.
   */
  initialize_profiling();

  /* All the main thread should do is start the master thread */
  pthread_create(&prof.master_id, NULL, &profile_master, NULL);
}

void deinitialize_profiling() {
  if(profopts.should_profile_all) {
    profile_all_deinit();
  }
}

void print_profiling() {
#if 0
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
#endif
}

void sh_stop_profile_master_thread() {
  pthread_join(prof.master_id, NULL);

  print_profiling();
  deinitialize_profiling();
}

