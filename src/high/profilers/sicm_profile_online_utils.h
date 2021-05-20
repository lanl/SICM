#pragma once
#include "sicm_packing.h"
#include <time.h>
#include <sys/time.h>
#include <signal.h>
#include <dirent.h>
#define _GNU_SOURCE
#include <unistd.h>
#include <sys/syscall.h>
#include <sys/types.h>
#include <numaif.h>

#define RESUME_SIG SIGUSR2
#define SUSPEND_SIG SIGUSR1

static sigset_t wait_mask;
static __thread int suspended = 0; // per-thread flag

void resume_handler(int sig) {
  pid_t self_tid;
  
  self_tid = syscall(SYS_gettid);
  
  suspended = 0;
}

void suspend_handler(int sig) {
  if (suspended) return;
  suspended = 1;
  do sigsuspend(&wait_mask); while (suspended);
}

/* Set up signal handlers so that we can suspend/resume any threads we please. */
void init_thread_suspension() {
  struct sigaction sa;

  sigfillset(&wait_mask);
  sigdelset(&wait_mask, SUSPEND_SIG);
  sigdelset(&wait_mask, RESUME_SIG);

  sigfillset(&sa.sa_mask);
  sa.sa_flags = 0;
  sa.sa_handler = resume_handler;
  sigaction(RESUME_SIG, &sa, NULL);

  sa.sa_handler = suspend_handler;
  sigaction(SUSPEND_SIG, &sa, NULL);
}

/* Uses proc to find all TIDs of the current process, and
   sends SIGSTOP to all of them. */
void suspend_all_threads() {
  DIR *proc_dir;
  char dirname[256];
  pid_t tid, self_tid;
  
  self_tid = syscall(SYS_gettid);
  sicm_arena_lock();

  snprintf(dirname, sizeof(dirname), "/proc/%d/task", getpid());
  proc_dir = opendir(dirname);
  
  if (proc_dir) {
    /*  /proc available, iterate through tasks... */
    struct dirent *entry;
    while ((entry = readdir(proc_dir)) != NULL) {
      if(entry->d_name[0] == '.')
      continue;
      tid = atoi(entry->d_name);
      if(tid != self_tid) {
        syscall(SYS_tkill, tid, SUSPEND_SIG);
      }
    }
    closedir(proc_dir);
  }
  
  sicm_arena_unlock();
}

/* Uses proc to find all TIDs of the current process, and
   sends SIGCONT to all of them. */
void resume_all_threads() {
  DIR *proc_dir;
  char dirname[256];
  pid_t tid, self_tid;
  
  self_tid = syscall(SYS_gettid);

  snprintf(dirname, sizeof(dirname), "/proc/%d/task", getpid());
  proc_dir = opendir(dirname);

  if (proc_dir) {
    /*  /proc available, iterate through tasks... */
    struct dirent *entry;
    while ((entry = readdir(proc_dir)) != NULL) {
      if(entry->d_name[0] == '.')
      continue;
      tid = atoi(entry->d_name);
      if(tid != self_tid) {
        syscall(SYS_tkill, tid, RESUME_SIG);
      }
    }
    closedir(proc_dir);
  }
}

/* Not only does this gather the RSS according to the SMAPS file, but 
   it also cross-references these values against all extents. */
void print_smaps_info() {
  char *line, *value_ptr,
       *region_start, *region_end,
       *extent_start, *extent_end,
       this_region_accounted_for;
  size_t value, size, i,
         present_accounted_pages,
         present_unaccounted_pages,
         num_pages;
  unsigned int pagesize;
  
  pthread_rwlock_rdlock(&tracker.extents_lock);
  
  get_online_data()->smaps_file = fopen("/proc/self/smaps", "r");
  if(!(get_online_data()->smaps_file)) {
    fprintf(stderr, "Failed to open smaps file. Aborting.\n");
    exit(1);
  }
  
  present_accounted_pages = 0;
  present_unaccounted_pages = 0;
  pagesize = sysconf(_SC_PAGE_SIZE);
  line = internal_malloc(64);
  size = 64;
  this_region_accounted_for = 0;
  while (getline(&line, &size, get_online_data()->smaps_file) > 0) {
    if(!strstr(line, "kB") && (sscanf(line, "%p-%p", &region_start, &region_end) == 2)) {
      /* Gets a line that defines a region */
      num_pages = (region_end - region_start) / pagesize;
      this_region_accounted_for = 0;
      extent_arr_for(tracker.extents, i) {
        
        /* Get the start and end of this extent */
        extent_start = tracker.extents->arr[i].start;
        extent_end = tracker.extents->arr[i].end;
        if(num_pages == 0) {
          continue;
        }
        
        if((extent_start >= region_start) && (extent_end <= region_end)) {
          this_region_accounted_for = 1;
          break;
        }
      }
      if(!this_region_accounted_for) {
        if(profopts.profile_online_debug_file) {
          fprintf(profopts.profile_online_debug_file,
                  "%s", line);
          fflush(profopts.profile_online_debug_file);
        }
      }
    } else if(strstr(line, "Rss:")) {
      /* Gets the RSS of the region that we saw on the last line */
      value_ptr = line + strlen("Rss:");
      if (sscanf(value_ptr, "%zu kB", &value) < 1) {
        fprintf(stderr, "Failed to get an RSS from smaps. Aborting.\n");
        exit(1);
      }
      if(this_region_accounted_for) {
        present_accounted_pages += (value / (pagesize / 1024));
      } else {
        if(profopts.profile_online_debug_file) {
          fprintf(profopts.profile_online_debug_file,
                  "%s", line);
          fflush(profopts.profile_online_debug_file);
        }
        present_unaccounted_pages += (value / (pagesize / 1024));
      }
    }
  }
  fseek(get_online_data()->smaps_file, 0, SEEK_SET);
  internal_free(line);
  
  if(profopts.profile_online_debug_file) {
    fprintf(profopts.profile_online_debug_file,
            "smaps accounted pages: %zu\n", present_accounted_pages);
    fprintf(profopts.profile_online_debug_file,
            "smaps unaccounted pages: %zu\n", present_unaccounted_pages);
    fflush(profopts.profile_online_debug_file);
  }
  
  pthread_rwlock_unlock(&tracker.extents_lock);
}

/* Calls move_pages to look at extents that aren't where we think they should be */
void print_move_pages_info() {
  char *start, *end, *ptr;
  unsigned int pagesize;
  unsigned long count;
  int *status, dev;
  void **pages;
  size_t error_pages, out_of_place_pages, i, n, total_extent_pages;
  arena_info *arena;
  
  pages = NULL;
  status = NULL;
  total_extent_pages = 0;
  out_of_place_pages = 0;
  error_pages = 0;
  pagesize = sysconf(_SC_PAGE_SIZE);
  extent_arr_for(tracker.extents, i) {
    arena = (arena_info *) tracker.extents->arr[i].arena;
    if(!arena) continue;
    
    /* Get the start and end of this extent */
    start = tracker.extents->arr[i].start;
    end = tracker.extents->arr[i].end;
    dev = get_online_arena_prof(arena->index)->dev;
    
    count = (end - start) / pagesize;
    if(count == 0) {
      continue;
    }
    
    /* Allocate room for and construct move_pages arguments */
    ptr = start;
    pages = internal_realloc(pages, count * sizeof(void *));
    n = 0;
    while(ptr < end) {
      pages[n] = ptr;
      ptr += pagesize;
      n++;
    }
    status = internal_realloc(status, count * sizeof(int));
    
    move_pages(0, count, pages, NULL, status, 0);
    
    /* Now count up the pages that aren't where they're supposed to be */
    for(n = 0; n < count; n++) {
      if(status[n] < 0) {
        error_pages++;
      } else if((dev == 0) && (status[n] != 1)) {
        /* It should be on NUMA node 1 */
        out_of_place_pages++;
      } else if((dev == 1) && (status[n] != 0)) {
        /* It should be on NUMA node 0 */
        out_of_place_pages++;
      }
    }
    total_extent_pages += count;
    
  }
  if(pages) {
    internal_free(pages);
  }
  if(status) {
    internal_free(status);
  }
  
  if(profopts.profile_online_debug_file) {
    fprintf(profopts.profile_online_debug_file,
            "move_pages out-of-place pages: %zu\n", out_of_place_pages);
    fprintf(profopts.profile_online_debug_file,
            "move_pages present pages: %zu\n", total_extent_pages - error_pages);
    fflush(profopts.profile_online_debug_file);
  }
}

/* Rebinds an arena to the device list.
   Times it and prints debugging information if necessary. */
void rebind_arena(int index, sicm_dev_ptr dl) {
  int retval;
  
  retval = sicm_arena_set_devices(tracker.arenas[index]->arena, dl);

  if(profopts.profile_online_debug_file) {
    if(retval == -EINVAL) {
      fprintf(profopts.profile_online_debug_file,
        "Rebinding arena %d failed in SICM.\n", index);
    } else if(retval != 0) {
      fprintf(profopts.profile_online_debug_file,
        "Rebinding arena %d failed internally.\n", index);
    }
  }
}

void full_rebind_bookkeeping(tree(site_info_ptr, int) sorted_sites) {
  get_online_prof()->reconfigure = 1;
  get_online_data()->prev_interval_reconfigure = 1;
  
  if(get_online_data()->prev_sorted_sites) {
    free_site_tree(get_online_data()->prev_sorted_sites);
  }
  if(get_online_data()->cur_sorted_sites) {
    get_online_data()->prev_sorted_sites = copy_site_tree(get_online_data()->cur_sorted_sites);
    free_site_tree(get_online_data()->cur_sorted_sites);
  }
  get_online_data()->cur_sorted_sites = copy_site_tree(sorted_sites);
}

/* Rebinds all arenas according to the `dev` and `hot` parameters
   in the arena's `per_arena_profile_online_info` struct. */
void full_rebind(tree(site_info_ptr, int) sorted_sites) {
  sicm_dev_ptr dl;
  int index;
  char dev, hot;
  tree_it(site_info_ptr, int) sit;
  struct timespec start, end, diff;

  if(profopts.profile_online_debug_file) {
    clock_gettime(CLOCK_MONOTONIC, &start);
  }
  
  tree_traverse(sorted_sites, sit) {
    index = tree_it_key(sit)->index;
    dev = get_online_arena_prof(index)->dev;
    hot = get_online_arena_prof(index)->hot;

    dl = NULL;
    if((hot == 0) || (hot == -1)) {
      dl = prof.profile_online.lower_dl;
      get_online_arena_prof(index)->dev = 0;
    }
    if(dl) {
      set_site_device(tree_it_val(sit), dl->devices[0]);
      rebind_arena(index, dl);
    }
  }
  
  tree_traverse(sorted_sites, sit) {
    index = tree_it_key(sit)->index;
    dev = get_online_arena_prof(index)->dev;
    hot = get_online_arena_prof(index)->hot;

    dl = NULL;
    if(hot == 1) {
      dl = prof.profile_online.upper_dl;
      get_online_arena_prof(index)->dev = 1;
    }
    if(dl) {
      set_site_device(tree_it_val(sit), dl->devices[0]);
      rebind_arena(index, dl);
    }
  }
  
  full_rebind_bookkeeping(sorted_sites);
  
  if(profopts.profile_online_debug_file) {
    clock_gettime(CLOCK_MONOTONIC, &end);
    timespec_diff(&start, &end, &diff);
    fprintf(profopts.profile_online_debug_file,
      "Full rebind estimate: %zu ms, real: %ld.%09ld s.\n",
      prof.profile_online.ski->penalty_move,
      diff.tv_sec, diff.tv_nsec);
    fflush(profopts.profile_online_debug_file);
  }
}
