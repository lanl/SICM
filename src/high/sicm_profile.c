#include <sys/types.h>
#include <unistd.h>
#include <fcntl.h>
#include <time.h>
#include <sys/time.h>
#include <sys/syscall.h>
#include <signal.h>
#include "sicm_profile.h"

profiler prof;
static int global_signal;

/* Returns 0 if "a" is bigger, 1 if "b" is bigger */
char timespec_cmp(struct timespec *a, struct timespec *b) {
	if (a->tv_sec == b->tv_sec) {
    if(a->tv_nsec > b->tv_nsec) {
      return 0;
    } else {
      return 1;
    }
	} else if(a->tv_sec > b->tv_sec) {
    return 0;
	} else {
    return 1;
  }
}

/* Subtracts two timespec structs from each other. Assumes stop is
 * larger than start.
 */
void timespec_diff(struct timespec *start, struct timespec *stop,                           
                   struct timespec *result)
{
  if ((stop->tv_nsec - start->tv_nsec) < 0) {
    result->tv_sec = stop->tv_sec - start->tv_sec - 1;                                      
    result->tv_nsec = stop->tv_nsec - start->tv_nsec + 1000000000;                          
  } else {
    result->tv_sec = stop->tv_sec - start->tv_sec;                                          
    result->tv_nsec = stop->tv_nsec - start->tv_nsec;                                       
  }
  return;
}


/* Allocates room for profiling information for this arena.
 * Returns a pointer to the profile_info struct as a void pointer
 * so that the arena can have a pointer to its profiling information.
 * Some profilers use this pointer to get to the profiling information
 * when all they have is a pointer to the arena.
 */
void *create_profile_arena(int index) {
  profile_info *profinfo;

  pthread_rwlock_wrlock(&prof.info_lock);

  profinfo = orig_calloc(1, sizeof(profile_info));

  if(profopts.should_profile_all) {
    profile_all_arena_init(&(profinfo->profile_all));
  }
  if(profopts.should_profile_rss) {
    profile_rss_arena_init(&(profinfo->profile_rss));
  }
  if(profopts.should_profile_extent_size) {
    profile_extent_size_arena_init(&(profinfo->profile_extent_size));
  }
  if(profopts.should_profile_allocs) {
    profile_allocs_arena_init(&(profinfo->profile_allocs));
  }
  if(profopts.should_profile_online) {
    profile_online_arena_init(&(profinfo->profile_online));
  }

  profinfo->num_intervals = 0;
  profinfo->first_interval = 0;
  prof.info[index] = profinfo;

  pthread_rwlock_unlock(&prof.info_lock);

  /* Return this so that the arena can have a pointer to its profiling
   * information
   */
  return (void *)prof.info[index];
}

void end_interval() {
  /* Signal the master thread that we're done */
  pthread_mutex_lock(&prof.mtx);
  prof.threads_finished++;
  pthread_cond_signal(&prof.cond);
  pthread_mutex_unlock(&prof.mtx);
}

/* This is the signal handler for the Master thread, so
 * it does this on every interval.
 */
void profile_master_interval(int s) {
	struct timespec start, end, target, actual;
  size_t i, n, x;
  char copy;

  /* Convenience pointers */
  profile_info *profinfo;
  arena_info *arena;
  profile_thread *profthread;

  pthread_rwlock_wrlock(&prof.info_lock);

  /* Start time */
  clock_gettime(CLOCK_MONOTONIC, &start);

  /* Increment the interval */
  arena_arr_for(i) {
    arena_check_good(arena, i);
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
    profthread = &prof.profile_threads[i];
    if(profthread->skipped_intervals == (profthread->skip_intervals - 1)) {
      /* This thread doesn't get skipped */
      pthread_kill(prof.profile_threads[i].id, prof.profile_threads[i].signal);
      profthread->skipped_intervals = 0;
    } else {
      /* This thread gets skipped */
      pthread_kill(prof.profile_threads[i].id, prof.profile_threads[i].skip_signal);
      profthread->skipped_intervals++;
    }
  }

  /* Wait for the threads to do their bit */
  pthread_mutex_lock(&prof.mtx);
  while(1) {
    if(prof.threads_finished) {
      /* At least one thread is finished, check if it's all of them */
      copy = prof.threads_finished;
      pthread_mutex_unlock(&prof.mtx);
      if(copy == prof.num_profile_threads) {
        /* They're all done. */
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
  pthread_mutex_unlock(&prof.mtx);

  /* End time */
  clock_gettime(CLOCK_MONOTONIC, &end);

  /* Throw a warning if this interval took too long */
  target.tv_sec = profopts.profile_rate_nseconds / 1000000000;
  target.tv_nsec = profopts.profile_rate_nseconds % 1000000000;
  timespec_diff(&start, &end, &actual);
  if(timespec_cmp(&target, &actual)) {
    fprintf(stderr, "WARNING: Interval (%ld.%09ld) went over the time limit (%ld.%09ld).\n",
            actual.tv_sec, actual.tv_nsec,
            target.tv_sec, target.tv_nsec);
  }

  /* Keep track of this interval's profiling values.
   * The profiling threads fill the value `tmp_accumulator`, and
   * this loop maintains the peak, total, and per-interval value.
   */
  arena_arr_for(i) {
    prof_check_good(arena, profinfo, i);

    if(profopts.should_profile_all) {
      profile_all_post_interval(profinfo);
    }
    if(profopts.should_profile_rss) {
      profile_rss_post_interval(profinfo);
    }
    if(profopts.should_profile_extent_size) {
      profile_extent_size_post_interval(profinfo);
    }
    if(profopts.should_profile_allocs) {
      profile_allocs_post_interval(profinfo);
    }
    if(profopts.should_profile_online) {
      profile_online_post_interval(profinfo);
    }

    /* Check if this arena contains the site that we're tracking,
     * print out some profiling information if so */
    for(n = 0; n < arena->num_alloc_sites; n++) {
      if(arena->alloc_sites[n] == tracker.track_site) {
        fprintf(stderr, "=====\n");
        fprintf(stderr, "Site %d\n", tracker.track_site);
        if(profopts.should_profile_all) {
          for(x = 0; x < profopts.num_profile_all_events; x++) {
            fprintf(stderr, "  Event: %s\n", profopts.profile_all_events[x]);
            fprintf(stderr, "    %zu\n", profinfo->profile_all.events[x].intervals[profinfo->num_intervals - 1]);
          }
        }
        if(profopts.should_profile_extent_size) {
          fprintf(stderr, "  Extent size:\n");
          fprintf(stderr, "    %zu\n", profinfo->profile_extent_size.intervals[profinfo->num_intervals - 1]);
        }
        if(profopts.should_profile_allocs) {
          fprintf(stderr, "  Allocations size:\n");
          fprintf(stderr, "    %zu\n", profinfo->profile_allocs.intervals[profinfo->num_intervals - 1]);
        }
        fprintf(stderr, "=====\n");
        break;
      }
    }
  }

  /* Finished handling this interval. Wait for another. */
  prof.cur_interval++;

  pthread_rwlock_unlock(&prof.info_lock);
}

/* Stops the master thread */
void profile_master_stop(int s) {
  size_t i;
  profile_thread *profthread;

  timer_delete(prof.timerid);

  /* Cancel all threads. Since threads can only be running while the master
   * thread is in a different signal handler from this one, it's impossible that they're
   * in an interval currently.
   */
  for(i = 0; i < prof.num_profile_threads; i++) {
    profthread = &prof.profile_threads[i];
    pthread_cancel(profthread->id);
    pthread_join(profthread->id, NULL);
  }

  pthread_exit(NULL);
}

void setup_profile_thread(void *(*main)(void *), /* Spinning loop function */
                          void (*interval)(int), /* Per-interval function */
                          void (*skip_interval)(int), /* Per-interval skip function */
                          unsigned long skip_intervals) {
  struct sigaction sa;
  profile_thread *profthread;

  /* Add a new profile_thread struct for it */
  prof.num_profile_threads++;
  prof.profile_threads = orig_realloc(prof.profile_threads, sizeof(profile_thread) * prof.num_profile_threads);
  profthread = &(prof.profile_threads[prof.num_profile_threads - 1]);

  /* Start the thread */
  pthread_create(&(profthread->id), NULL, main, NULL);

  /* Set up the signal handler */
  profthread->signal = global_signal;
	sa.sa_flags = 0;
  sa.sa_handler = interval;
  sigemptyset(&sa.sa_mask);
  if(sigaction(profthread->signal, &sa, NULL) == -1) {
    fprintf(stderr, "Error creating signal handler for signal %d. Aborting: %s\n", profthread->signal, strerror(errno));
    exit(1);
  }
  global_signal++;

  /* Set up the signal handler for what gets called if we're skipping this interval */
  profthread->skip_signal = global_signal;
	sa.sa_flags = 0;
  sa.sa_handler = skip_interval;
  sigemptyset(&sa.sa_mask);
  if(sigaction(profthread->skip_signal, &sa, NULL) == -1) {
    fprintf(stderr, "Error creating signal handler for signal %d. Aborting: %s\n", profthread->skip_signal, strerror(errno));
    exit(1);
  }
  global_signal++;

  profthread->skipped_intervals = 0;
  profthread->skip_intervals = skip_intervals;
}

/* This is the Master thread, it keeps track of intervals
 * and starts/stops the profiling threads. It has a timer
 * which signals it at a certain interval. Each time this
 * happens, it notifies the profiling threads.
 */
void *profile_master(void *a) {
  struct sigevent sev;
  struct sigaction sa;
  struct itimerspec its;
  long long frequency;
  sigset_t mask;
  pid_t tid;

  if(profopts.should_profile_all) {
    setup_profile_thread(&profile_all, 
                         &profile_all_interval, 
                         &profile_all_skip_interval, 
                         profopts.profile_all_skip_intervals);
  }
  if(profopts.should_profile_rss) {
    setup_profile_thread(&profile_rss, 
                         &profile_rss_interval, 
                         &profile_rss_skip_interval, 
                         profopts.profile_rss_skip_intervals);
  }
  if(profopts.should_profile_extent_size) {
    setup_profile_thread(&profile_extent_size, 
                         &profile_extent_size_interval, 
                         &profile_extent_size_skip_interval, 
                         profopts.profile_extent_size_skip_intervals);
  }
  if(profopts.should_profile_allocs) {
    setup_profile_thread(&profile_allocs, 
                         &profile_allocs_interval, 
                         &profile_allocs_skip_interval, 
                         profopts.profile_allocs_skip_intervals);
  }
  if(profopts.should_profile_online) {
    setup_profile_thread(&profile_online, 
                         &profile_online_interval, 
                         &profile_online_skip_interval, 
                         profopts.profile_online_skip_intervals);
  }
  
  /* Initialize synchronization primitives */
  pthread_mutex_init(&prof.mtx, NULL);
  pthread_cond_init(&prof.cond, NULL);
  prof.cur_interval = 0;

  /* Set up a signal handler for the master */
  sa.sa_flags = 0;
  sa.sa_handler = profile_master_interval;
  sigemptyset(&sa.sa_mask);
  sigaddset(&sa.sa_mask, prof.stop_signal); /* Stop signal should block until an interval is finished */
  if(sigaction(prof.master_signal, &sa, NULL) == -1) {
    fprintf(stderr, "Error creating signal handler. Aborting.\n");
    exit(1);
  }

  /* Block the signal for a bit */
  sigemptyset(&mask);
  sigaddset(&mask, prof.master_signal);
  if(sigprocmask(SIG_SETMASK, &mask, NULL) == -1) {
    fprintf(stderr, "Error blocking signal. Aborting.\n");
    exit(1);
  }

  /* Create the timer */
  tid = syscall(SYS_gettid);
  sev.sigev_notify = SIGEV_THREAD_ID;
  sev.sigev_signo = prof.master_signal;
  sev.sigev_value.sival_ptr = &prof.timerid;
  sev._sigev_un._tid = tid;
  if(timer_create(CLOCK_REALTIME, &sev, &prof.timerid) == -1) {
    fprintf(stderr, "Error creating timer. Aborting.\n");
    exit(1);
  }
  
  /* Set the timer */
  its.it_value.tv_sec = profopts.profile_rate_nseconds / 1000000000;
  its.it_value.tv_nsec = profopts.profile_rate_nseconds % 1000000000;
  its.it_interval.tv_sec = its.it_value.tv_sec;
  its.it_interval.tv_nsec = its.it_value.tv_nsec;
  if(timer_settime(prof.timerid, 0, &its, NULL) == -1) {
    fprintf(stderr, "Error setting the timer. Aborting.\n");
    exit(1);
  }

  /* Unblock the signal */
  if(sigprocmask(SIG_UNBLOCK, &mask, NULL) == -1) {
    fprintf(stderr, "Error unblocking signal. Aborting.\n");
    exit(1);
  }

  /* Wait for either the timer to signal us to start a new interval,
   * or for the main thread to signal us to stop.
   */
  while(1) {}
}

void initialize_profiling() {
  pthread_rwlock_init(&(prof.info_lock), NULL);

  /* Allocate room for the per-arena profiling information */
  prof.info = orig_calloc(tracker.max_arenas, sizeof(profile_info *));

  prof.threads_finished = 0;

  /* The signal that will stop the master thread */
  global_signal = SIGRTMIN;
  prof.stop_signal = global_signal;
  global_signal++;

  /* The signal that the master thread will use to tell itself
   * (via a timer) when the next interval should start */
  prof.master_signal = global_signal;
  global_signal++;

  /* All of this initialization HAS to happen in the main SICM thread.
   * If it's not, the `perf_event_open` system call won't profile
   * the current thread, but instead will only profile the thread that
   * it was run in.
   */
  if(profopts.should_profile_all) {
    profile_all_init();
  }
  if(profopts.should_profile_rss) {
    profile_rss_init();
  }
  if(profopts.should_profile_extent_size) {
    profile_extent_size_init();
  }
  if(profopts.should_profile_allocs) {
    profile_allocs_init();
  }
  if(profopts.should_profile_online) {
    profile_online_init();
  }
}

void sh_start_profile_master_thread() {
  struct sigaction sa;

  /* This initializes the values that the threads will need to do their profiling,
   * including perf events, file descriptors, etc.
   */
  initialize_profiling();

  /* Set up the signal that we'll use to stop the master thread */
  sa.sa_flags = 0;
  sa.sa_handler = profile_master_stop;
  sigemptyset(&sa.sa_mask);
  sigaddset(&sa.sa_mask, prof.master_signal); /* Block the interval signal while the stop signal handler is running */
  if(sigaction(prof.stop_signal, &sa, NULL) == -1) {
    fprintf(stderr, "Error creating master stop signal handler. Aborting.\n");
    exit(1);
  }

  /* Start the master thread */
  pthread_create(&prof.master_id, NULL, &profile_master, NULL);
}

void deinitialize_profiling() {
  if(profopts.should_profile_all) {
    profile_all_deinit();
  }
}

void print_profiling() {
  size_t i, n, x;
  profile_info *profinfo;
  arena_info *arena;

  printf("===== PROFILING INFORMATION =====\n");
  arena_arr_for(i) {
    prof_check_good(arena, profinfo, i);

    /* Print the sites that are in this arena */
    printf("%d sites: ", arena->num_alloc_sites);
    for(n = 0; n < arena->num_alloc_sites; n++) {
      printf("%d ", arena->alloc_sites[n]);
    }
    printf("\n");

    /* General info */
    printf("    Number of intervals: %zu\n", profinfo->num_intervals);
    printf("    First interval: %zu\n", profinfo->first_interval);

    /* profile_rss */
    if(profopts.should_profile_rss) {
      printf("  RSS:\n");
      printf("    Peak: %zu\n", profinfo->profile_rss.peak);
      for(x = 0; x < profinfo->num_intervals; x++) {
        printf("    %zu\n", profinfo->profile_rss.intervals[x]);
      }
    }

    /* profile_extent_size */
    if(profopts.should_profile_extent_size) {
      printf("  Extents size:\n");
      printf("    Peak: %zu\n", profinfo->profile_extent_size.peak);
      for(x = 0; x < profinfo->num_intervals; x++) {
        printf("    %zu\n", profinfo->profile_extent_size.intervals[x]);
      }
    }

    /* profile_allocs */
    if(profopts.should_profile_allocs) {
      printf("  Allocations size:\n");
      printf("    Peak: %zu\n", profinfo->profile_allocs.peak);
      for(x = 0; x < profinfo->num_intervals; x++) {
        printf("    %zu\n", profinfo->profile_allocs.intervals[x]);
      }
    }

    /* profile_all events */
    if(profopts.should_profile_all) {
      for(n = 0; n < profopts.num_profile_all_events; n++) {
        printf("  Event: %s\n", profopts.profile_all_events[n]);
        printf("    Total: %zu\n", profinfo->profile_all.events[n].total);
        printf("    Peak: %zu\n", profinfo->profile_all.events[n].peak);
        for(x = 0; x < profinfo->num_intervals; x++) {
          printf("      %zu\n", profinfo->profile_all.events[n].intervals[x]);
        }
      }
    }
  }
  printf("===== END PROFILING INFORMATION =====\n");
  fflush(stdout);

#if 0
  /* MBI profiling */
  } else if(profopts.should_profile_one) {
    printf("===== MBI RESULTS FOR SITE %u =====\n", profopts.profile_one_site);
    printf("Average bandwidth: %.1f MB/s\n", prof.running_avg);
    printf("Maximum bandwidth: %.1f MB/s\n", prof.max_bandwidth);
    if(profopts.should_profile_rss) {
      printf("Peak RSS: %zu\n", tracker.arenas[profopts.profile_one_site]->peak_rss);
    }
    printf("===== END MBI RESULTS =====\n");
#endif
}

void sh_stop_profile_master_thread() {
  /* Tell the master thread to stop */
  pthread_kill(prof.master_id, prof.stop_signal);
  pthread_join(prof.master_id, NULL);

  if(!profopts.should_profile_online) {
    print_profiling();
  }
  deinitialize_profiling();
}
