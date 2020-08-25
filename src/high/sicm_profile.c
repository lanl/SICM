#include <sys/types.h>
#include <unistd.h>
#include <fcntl.h>
#include <time.h>
#include <sys/time.h>
#include <sys/syscall.h>
#include <signal.h>

#define SICM_RUNTIME 1
#include "sicm_profile.h"
#include "sicm_parsing.h"

profiler prof;
static pthread_rwlock_t profile_lock = PTHREAD_RWLOCK_INITIALIZER;
static int global_signal;

/* Runs when an arena has already been created, but the runtime library
   has added an allocation site to the arena. */
void add_site_profile(int index, int site_id) {
  arena_profile *aprof;
  
  pthread_rwlock_wrlock(&profile_lock);
  if((index < 0) || (index > tracker.max_arenas)) {
    fprintf(stderr, "Can't add a site to an index (%d) larger than the maximum index (%d).\n", index, tracker.max_arenas);
    exit(1);
  }
  aprof = get_arena_prof(index);
  if(!aprof) {
    fprintf(stderr, "Tried to add a site to index %d without having created an arena profile there. Aborting.\n", index);
    exit(1);
  }
  if(aprof->num_alloc_sites + 1 > tracker.max_sites_per_arena) {
    fprintf(stderr, "The maximum number of sites per arena has been reached: %d\n", tracker.max_sites_per_arena);
    exit(1);
  }
  aprof->alloc_sites[aprof->num_alloc_sites - 1] = site_id;
  aprof->num_alloc_sites++;
  pthread_rwlock_unlock(&profile_lock);
}

/* Runs when a new arena is created. Allocates room to store
   profiling information about this arena. */
void create_arena_profile(int index, int site_id) {
  arena_profile *aprof;

  pthread_rwlock_wrlock(&profile_lock);
  
  if((index < 0) || (index > tracker.max_arenas)) {
    fprintf(stderr, "Can't add a site to an index (%d) larger than the maximum index (%d).\n", index, tracker.max_arenas);
    exit(1);
  }
  
  aprof = orig_calloc(1, sizeof(arena_profile));

  if(should_profile_all()) {
    profile_all_arena_init(&(aprof->profile_all));
  }
  if(should_profile_rss()) {
    profile_rss_arena_init(&(aprof->profile_rss));
  }
  if(should_profile_extent_size()) {
    profile_extent_size_arena_init(&(aprof->profile_extent_size));
  }
  if(should_profile_allocs()) {
    profile_allocs_arena_init(&(aprof->profile_allocs));
  }
  if(should_profile_online()) {
    profile_online_arena_init(&(aprof->profile_online));
  }

  /* Creates a profile for this arena at the current interval */
  aprof->index = index;
  aprof->num_alloc_sites = 1;
  aprof->alloc_sites = orig_malloc(sizeof(int) * tracker.max_sites_per_arena);
  aprof->alloc_sites[0] = site_id;
  prof.profile->this_interval.arenas[index] = aprof;
  prof.profile->this_interval.num_arenas++;
  if(index > prof.profile->this_interval.max_index) {
    prof.profile->this_interval.max_index = index;
  }
  
  pthread_rwlock_unlock(&profile_lock);
}

/* This is the signal handler for the Master thread, so
 * it does this on every interval.
 */
void profile_master_interval(int s) {
  struct timespec actual;
  size_t i, n, x;
  char copy;
  double elapsed_time;

  /* Convenience pointers */
  arena_profile *aprof;
  arena_info *arena;
  profile_thread *profthread;
  
  pthread_rwlock_wrlock(&profile_lock);
  
  /* Here, we're checking to see if the time between this interval and
     the previous one is too short. If it is, this is likely a queued-up
     signal caused by an interval that took too long. In some cases,
     profiling threads can take up to 10 seconds to complete, and in that
     span of time, hundreds or thousands of timer signals could have been
     queued up. We want to prevent that from happening, so ignore a signal
     that occurs quicker than it should. */
  clock_gettime(CLOCK_MONOTONIC, &(prof.start));
  timespec_diff(&(prof.end), &(prof.start), &actual);
  elapsed_time = actual.tv_sec + (((double) actual.tv_nsec) / 1000000000);
  if(elapsed_time < (prof.target - (prof.target * 10 / 100))) {
    /* It's too soon since the last interval. */
    clock_gettime(CLOCK_MONOTONIC, &(prof.end));
    return;
  }

  /* Call the interval functions for each of the profiling types */
  for(i = 0; i < prof.num_profile_threads; i++) {
    profthread = &prof.profile_threads[i];
    if(profthread->skipped_intervals == (profthread->skip_intervals - 1)) {
      /* This thread doesn't get skipped */
      (*profthread->interval_func)(0);
      profthread->skipped_intervals = 0;
    } else {
      /* This thread gets skipped */
      (*profthread->skip_interval_func)(0);
      profthread->skipped_intervals++;
    }
  }

  aprof_arr_for(i, aprof) {
    aprof_check_good(i, aprof);

    if(should_profile_all()) {
      profile_all_post_interval(aprof);
    }
    if(should_profile_rss()) {
      profile_rss_post_interval(aprof);
    }
    if(should_profile_extent_size()) {
      profile_extent_size_post_interval(aprof);
    }
    if(should_profile_allocs()) {
      profile_allocs_post_interval(aprof);
    }
    if(should_profile_online()) {
      profile_online_post_interval(aprof);
    }
  }
  if(should_profile_bw()) {
    profile_bw_post_interval();
  }
  if(should_profile_latency()) {
    profile_latency_post_interval();
  }
  
  /* Store the time that this interval took */
  clock_gettime(CLOCK_MONOTONIC, &(prof.end));
  timespec_diff(&(prof.start), &(prof.end), &actual);
  prof.profile->this_interval.time = actual.tv_sec + (((double) actual.tv_nsec) / 1000000000);
  
  /* End the interval */
  if(prof.profile->num_intervals) {
    /* If we've had at least one interval of profiling already,
       store that pointer in `prev_interval` */
    prof.prev_interval = &(prof.profile->intervals[prof.profile->num_intervals - 1]);
  }
  prof.profile->num_intervals++;
  copy_interval_profile(prof.profile->num_intervals - 1);
  prof.cur_interval = &(prof.profile->intervals[prof.profile->num_intervals - 1]);
  
  /* We need this timer to actually end outside out of the lock */
  clock_gettime(CLOCK_MONOTONIC, &(prof.end));
  
  pthread_rwlock_unlock(&profile_lock);
}

/* Stops the master thread */
void profile_master_stop(int s) {
  timer_delete(prof.timerid);
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

  profthread->interval_func      = interval;
  profthread->skip_interval_func = skip_interval;
  profthread->skipped_intervals  = 0;
  profthread->skip_intervals     = skip_intervals;
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
  
  /* NOTE: This order is important for profiling types that depend on others.
     For example, if a profiling type depends on the bandwidth values, 
     make sure that its `setup_profile_thread` is called *before* the bandwidth
     profiler. This also means that, if you use the SH_PROFILE_SEPARATE_THREADS feature,
     you must add mutices to ensure that one type has finished before another starts. */
  
  if(should_profile_latency()) {
    setup_profile_thread(&profile_latency,
                         &profile_latency_interval,
                         &profile_latency_skip_interval,
                         profopts.profile_latency_skip_intervals);
  }
  if(should_profile_all()) {
    setup_profile_thread(&profile_all,
                         &profile_all_interval,
                         &profile_all_skip_interval,
                         profopts.profile_all_skip_intervals);
  }
  if(should_profile_rss()) {
    setup_profile_thread(&profile_rss,
                         &profile_rss_interval,
                         &profile_rss_skip_interval,
                         profopts.profile_rss_skip_intervals);
  }
  if(should_profile_bw()) {
    setup_profile_thread(&profile_bw,
                         &profile_bw_interval,
                         &profile_bw_skip_interval,
                         profopts.profile_bw_skip_intervals);
  }
  if(should_profile_extent_size()) {
    setup_profile_thread(&profile_extent_size,
                         &profile_extent_size_interval,
                         &profile_extent_size_skip_interval,
                         profopts.profile_extent_size_skip_intervals);
  }
  if(should_profile_allocs()) {
    setup_profile_thread(&profile_allocs,
                         &profile_allocs_interval,
                         &profile_allocs_skip_interval,
                         profopts.profile_allocs_skip_intervals);
  }
  if(should_profile_online()) {
    setup_profile_thread(&profile_online,
                         &profile_online_interval,
                         &profile_online_skip_interval,
                         profopts.profile_online_skip_intervals);
  }

  /* Initialize synchronization primitives */
  pthread_mutex_init(&prof.mtx, NULL);
  pthread_cond_init(&prof.cond, NULL);

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
  
  /* Store how long the interval should take */
  prof.target = ((double) profopts.profile_rate_nseconds) / 1000000000;

  /* Set the timer */
  its.it_value.tv_sec     = profopts.profile_rate_nseconds / 1000000000;
  its.it_value.tv_nsec    = profopts.profile_rate_nseconds % 1000000000;
  its.it_interval.tv_sec  = its.it_value.tv_sec;
  its.it_interval.tv_nsec = its.it_value.tv_nsec;
  if(timer_settime(prof.timerid, 0, &its, NULL) == -1) {
    fprintf(stderr, "Error setting the timer. Aborting.\n");
    exit(1);
  }
  
  /* Initialize this time */
  clock_gettime(CLOCK_MONOTONIC, &(prof.end));
  
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

void init_application_profile(application_profile *profile) {
  prof.profile->num_intervals = 0;
  prof.profile->intervals = NULL;
  
  /* Set flags for what type of profiling we'll store */
  if(should_profile_all()) {
    prof.profile->has_profile_all = 1;
  }
  if(should_profile_rss()) {
    prof.profile->has_profile_rss = 1;
  }
  if(should_profile_allocs()) {
    prof.profile->has_profile_allocs = 1;
  }
  if(should_profile_extent_size()) {
    prof.profile->has_profile_extent_size = 1;
  }
  if(should_profile_online()) {
    prof.profile->has_profile_online = 1;
  }
  if(should_profile_bw()) {
    prof.profile->has_profile_bw = 1;
  }
  if(should_profile_latency()) {
    prof.profile->has_profile_latency = 1;
  }
  if(profopts.profile_bw_relative) {
    prof.profile->has_profile_bw_relative = 1;
  }
}

void initialize_profiling() {
  size_t i;
  
  pthread_rwlock_wrlock(&profile_lock);

  /* Initialize the structs that store the profiling information */
  prof.profile = orig_calloc(1, sizeof(application_profile));
  init_application_profile(prof.profile);

  prof.cur_interval = NULL;
  prof.prev_interval = NULL;

  /* Stores the current interval's profiling */
  prof.profile->this_interval.num_arenas = 0;
  prof.profile->this_interval.max_index = 0;
  prof.profile->this_interval.arenas = orig_calloc(tracker.max_arenas, sizeof(arena_profile *));

  /* Store the profile_all event strings */
  prof.profile->num_profile_all_events = profopts.num_profile_all_events;
  prof.profile->profile_all_events = orig_calloc(prof.profile->num_profile_all_events, sizeof(char *));
  for(i = 0; i < profopts.num_profile_all_events; i++) {
    prof.profile->profile_all_events[i] = orig_malloc((strlen(profopts.profile_all_events[i]) + 1) * sizeof(char));
    strcpy(prof.profile->profile_all_events[i], profopts.profile_all_events[i]);
  }
  
  /* Store which sockets we profiled */
  prof.profile->num_profile_skts = profopts.num_profile_skt_cpus;
  prof.profile->profile_skts = orig_calloc(prof.profile->num_profile_skts, sizeof(int));
  for(i = 0; i < profopts.num_profile_skt_cpus; i++) {
    prof.profile->profile_skts[i] = profopts.profile_skts[i];
  }
  
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
  if(should_profile_all()) {
    profile_all_init();
  }
  if(should_profile_rss()) {
    profile_rss_init();
  }
  if(should_profile_bw()) {
    profile_bw_init();
  }
  if(should_profile_latency()) {
    profile_latency_init();
  }
  if(should_profile_extent_size()) {
    profile_extent_size_init();
  }
  if(should_profile_allocs()) {
    profile_allocs_init();
  }
  if(should_profile_online()) {
    profile_online_init();
  }
  
  pthread_rwlock_unlock(&profile_lock);
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
  if(should_profile_all()) {
    profile_all_deinit();
  }
  if(should_profile_rss()) {
    profile_rss_deinit();
  }
  if(should_profile_bw()) {
    profile_bw_deinit();
  }
  if(should_profile_latency()) {
    profile_latency_deinit();
  }
  if(should_profile_extent_size()) {
    profile_extent_size_deinit();
  }
  if(should_profile_allocs()) {
    profile_allocs_deinit();
  }
  if(should_profile_online()) {
    profile_online_deinit();
  }
}

void sh_stop_profile_master_thread() {
  /* Tell the master thread to stop */
  pthread_kill(prof.master_id, prof.stop_signal);
  pthread_join(prof.master_id, NULL);

  if(profopts.profile_output_file) {
    sh_print_profiling(prof.profile, profopts.profile_output_file);
  }
  deinitialize_profiling();
}
