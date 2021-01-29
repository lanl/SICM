#include "sicm_packing.h"
#include <time.h>
#include <sys/time.h>
#include <signal.h>
#include <dirent.h>
#define _GNU_SOURCE
#include <unistd.h>
#include <sys/syscall.h>
#include <sys/types.h>

#define RESUME_SIG SIGUSR2
#define SUSPEND_SIG SIGUSR1

static sigset_t wait_mask;
static __thread int suspended = 0; // per-thread flag

void resume_handler(int sig) {
  pid_t self_tid;
  
  self_tid = syscall(SYS_gettid);
  if(profopts.profile_online_debug_file) {
    /*
    fprintf(profopts.profile_online_debug_file,
      "Inside the thread, resuming %ld.\n", (long) self_tid);
    fflush(profopts.profile_online_debug_file);
    */
  }
  
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
  
  if(profopts.profile_online_debug_file) {
    fprintf(profopts.profile_online_debug_file,
      "Own TID %ld.\n", (long) self_tid);
    fflush(profopts.profile_online_debug_file);
  }

  if (proc_dir) {
    /*  /proc available, iterate through tasks... */
    struct dirent *entry;
    while ((entry = readdir(proc_dir)) != NULL) {
      if(entry->d_name[0] == '.')
      continue;
      tid = atoi(entry->d_name);
      if(tid != self_tid) {
        syscall(SYS_tkill, tid, SUSPEND_SIG);
        if(profopts.profile_online_debug_file) {
          fprintf(profopts.profile_online_debug_file,
            "Suspend: %ld.\n", (long) tid);
          fflush(profopts.profile_online_debug_file);
        }
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
        if(profopts.profile_online_debug_file) {
          fprintf(profopts.profile_online_debug_file,
            "Resume: %ld.\n", (long) tid);
          fflush(profopts.profile_online_debug_file);
        }
      }
    }
    closedir(proc_dir);
  }
}

/* Makes a deep copy of the given hotset. */
tree(int, site_info_ptr) copy_hotset(tree(int, site_info_ptr) hotset) {
  tree(int, site_info_ptr) retval;
  tree_it(int, site_info_ptr) sit;
  site_info_ptr tmp;
  
  retval = tree_make(int, site_info_ptr);
  tree_traverse(hotset, sit) {
    /* Make a copy of site site_info struct */
    tmp = malloc(sizeof(site_profile_info));
    memcpy(tmp, tree_it_val(sit), sizeof(site_profile_info));
    tree_insert(retval, tree_it_key(sit), tmp);
  }
  
  return retval;
}

/* Makes a deep copy of the given site tree. */
tree(site_info_ptr, int) copy_sorted_sites(tree(site_info_ptr, int) sorted_sites) {
  tree(site_info_ptr, int) retval;
  tree_it(site_info_ptr, int) sit;
  site_info_ptr tmp;
  
  retval = tree_make_c(site_info_ptr, int, &site_tree_cmp);
  tree_traverse(sorted_sites, sit) {
    /* Make a copy of site site_info struct */
    tmp = malloc(sizeof(site_profile_info));
    memcpy(tmp, tree_it_key(sit), sizeof(site_profile_info));
    tree_insert(retval, tmp, tree_it_val(sit));
  }
  
  return retval;
}

/* Rebinds an arena to the device list.
   Times it and prints debugging information if necessary. */
void rebind_arena(int index, sicm_dev_ptr dl, tree_it(site_info_ptr, int) sit) {
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
  
  suspend_all_threads();

  tree_traverse(sorted_sites, sit) {
    index = tree_it_key(sit)->index;
    dev = get_arena_online_prof(index)->dev;
    hot = get_arena_online_prof(index)->hot;

    dl = NULL;
    if((dev == 1) && (hot == 0)) {
      /* The site is in DRAM and isn't in the hotset */
      dl = prof.profile_online.lower_dl;
      get_arena_online_prof(index)->dev = 0;
    }
    if(((dev == 0) || (dev == -1)) && !hot) {
      dl = prof.profile_online.lower_dl;
      get_arena_online_prof(index)->dev = 0;
    }
    if(dl) {
      rebind_arena(index, dl, sit);
    }
  }
  
  tree_traverse(sorted_sites, sit) {
    index = tree_it_key(sit)->index;
    dev = get_arena_online_prof(index)->dev;
    hot = get_arena_online_prof(index)->hot;

    dl = NULL;
    if(((dev == -1) && hot) ||
        ((dev == 0) && hot)) {
      /* The site is in AEP, and is in the hotset. */
      dl = prof.profile_online.upper_dl;
      get_arena_online_prof(index)->dev = 1;
    }
    if((dev == 1) && hot) {
      dl = prof.profile_online.upper_dl;
      get_arena_online_prof(index)->dev = 1;
    }
    if(dl) {
      rebind_arena(index, dl, sit);
    }
  }
  
  resume_all_threads();
  
  if(profopts.profile_online_debug_file) {
    clock_gettime(CLOCK_MONOTONIC, &end);
    timespec_diff(&start, &end, &diff);
    if(profopts.profile_online_ski) {
      fprintf(profopts.profile_online_debug_file,
        "Full rebind estimate: %zu ms, real: %ld.%09ld s.\n",
        prof.profile_online.ski->penalty_move,
        diff.tv_sec, diff.tv_nsec);
      fflush(profopts.profile_online_debug_file);
    }
  }
}

/* Ignores the `dev` parameter, and just rebinds all sites according to their
   `hot` parameter. Intended to be used when the online approach first kicks in,
    since at that point, the `dev` parameter hasn't been set yet (and all sites
    are defaulting to the upper tier). */
void full_rebind_first(tree(site_info_ptr, int) sorted_sites) {
  sicm_dev_ptr dl;
  int index;
  tree_it(site_info_ptr, int) sit;
  struct timespec start, end, diff;
  char hot;

  if(profopts.profile_online_debug_file) {
    clock_gettime(CLOCK_MONOTONIC, &start);
  }
  
  suspend_all_threads();

  /* Rebind all of the cold arenas first */
  tree_traverse(sorted_sites, sit) {
    index = tree_it_key(sit)->index;
    hot = get_arena_online_prof(index)->hot;

    dl = NULL;
    if(!hot) {
      dl = prof.profile_online.lower_dl;
      get_arena_online_prof(index)->dev = 0;
    }
    
    if(dl) {
      if(profopts.profile_online_debug_file) {
        fprintf(profopts.profile_online_debug_file, "Rebinding site %d (arena %d) down.\n", tree_it_val(sit), index);
      }
      set_site_device(tree_it_val(sit), dl->devices[0]);
      rebind_arena(index, dl, sit);
    }
  }
  
  /* Now rebind the hot ones */
  tree_traverse(sorted_sites, sit) {
    index = tree_it_key(sit)->index;
    hot = get_arena_online_prof(index)->hot;

    dl = NULL;
    if(hot) {
      dl = prof.profile_online.upper_dl;
      get_arena_online_prof(index)->dev = 1;
    }
    
    if(dl) {
      if(profopts.profile_online_debug_file) {
        fprintf(profopts.profile_online_debug_file, "Rebinding site %d (arena %d) up.\n", tree_it_val(sit), index);
      }
      set_site_device(tree_it_val(sit), dl->devices[0]);
      rebind_arena(index, dl, sit);
    }
  }
  
  resume_all_threads();
  
  if(profopts.profile_online_debug_file) {
    clock_gettime(CLOCK_MONOTONIC, &end);
    timespec_diff(&start, &end, &diff);
    if(profopts.profile_online_ski) {
      fprintf(profopts.profile_online_debug_file,
        "Full rebind estimate: %zu ms, real: %ld.%09ld s.\n",
        prof.profile_online.ski->penalty_move,
        diff.tv_sec, diff.tv_nsec);
      fflush(profopts.profile_online_debug_file);
    }
  }
}
