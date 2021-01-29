#ifndef _LARGEFILE64_SOURCE
#define _LARGEFILE64_SOURCE
#endif

#include <sys/stat.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <sys/syscall.h>
#include <errno.h>
#include <unistd.h>
#include <sys/types.h>
#include <stdint.h>

#define SICM_RUNTIME 1
#include "sicm_runtime.h"
#include "sicm_profilers.h"
#include "sicm_profile.h"

void profile_objmap_arena_init(per_arena_profile_objmap_info *);
void profile_objmap_deinit();
void profile_objmap_init();
void *profile_objmap(void *);
void profile_objmap_interval(int);
void profile_objmap_skip_interval(int);
void profile_objmap_post_interval(arena_profile *);

#define RSS "Rss:"

unsigned long long get_cgroup_memory_unaccounted_not_objmap() {
  unsigned long long retval;
  size_t size;
  char *line;
  ssize_t err;
  
  prof.profile_objmap.memory_unaccounted_not_objmap_file = fopen("/sys/fs/cgroup/0/memory.unaccounted_not_objmap", "r");
  if(!prof.profile_objmap.memory_unaccounted_not_objmap_file) {
    fprintf(stderr, "Failed to open memory_unaccounted_not_objmap file. Continuing anyway.\n");
    return 0;
  }
  
  retval = 0;
  if(prof.profile_objmap.memory_unaccounted_not_objmap_file) {
    line = NULL;
    size = 0;
    err = getline(&line, &size, prof.profile_objmap.memory_unaccounted_not_objmap_file);
    if(err < 0) {
      return 0;
    }
    retval = strtoull(line, NULL, 10);
  }
  
  fclose(prof.profile_objmap.memory_unaccounted_not_objmap_file);
  
  return retval;
}

unsigned long long get_cgroup_memory_current() {
  unsigned long long retval;
  size_t size;
  char *line;
  ssize_t err;
  
  prof.profile_objmap.memory_current_file = fopen("/sys/fs/cgroup/0/memory.current", "r");
  if(!prof.profile_objmap.memory_current_file) {
    fprintf(stderr, "Failed to open memory_current file. Continuing anyway.\n");
    return 0;
  }
  
  retval = 0;
  if(prof.profile_objmap.memory_current_file) {
    line = NULL;
    size = 0;
    err = getline(&line, &size, prof.profile_objmap.memory_current_file);
    if(err < 0) {
      return 0;
    }
    retval = strtoull(line, NULL, 10);
  }
  
  fclose(prof.profile_objmap.memory_current_file);
  
  return retval;
}

unsigned long long get_cgroup_node0_current() {
  unsigned long long retval;
  size_t size;
  char *line;
  ssize_t err;
  
  prof.profile_objmap.node0_current_file = fopen("/sys/fs/cgroup/0/memory.node0_current", "r");
  if(!prof.profile_objmap.node0_current_file) {
    fprintf(stderr, "Failed to open node0_current file. Continuing anyway.\n");
    return 0;
  }
  
  retval = 0;
  if(prof.profile_objmap.node0_current_file) {
    line = NULL;
    size = 0;
    err = getline(&line, &size, prof.profile_objmap.node0_current_file);
    if(err < 0) {
      return 0;
    }
    retval = strtoull(line, NULL, 10);
  }
  
  fclose(prof.profile_objmap.node0_current_file);
  
  return retval;
}

unsigned long long get_cgroup_node1_current() {
  unsigned long long retval;
  size_t size;
  char *line;
  ssize_t err;
  
  prof.profile_objmap.node1_current_file = fopen("/sys/fs/cgroup/0/memory.node1_current", "r");
  if(!prof.profile_objmap.node1_current_file) {
    fprintf(stderr, "Failed to open node1_current file. Continuing anyway.\n");
    return 0;
  }
  
  retval = 0;
  if(prof.profile_objmap.node1_current_file) {
    line = NULL;
    size = 0;
    err = getline(&line, &size, prof.profile_objmap.node1_current_file);
    if(err < 0) {
      return 0;
    }
    retval = strtoull(line, NULL, 10);
  }
  
  fclose(prof.profile_objmap.node1_current_file);
  
  return retval;
}

unsigned long long get_cgroup_node0_max() {
  unsigned long long retval;
  size_t size;
  char *line;
  ssize_t err;
  
  prof.profile_objmap.node0_max_file = fopen("/sys/fs/cgroup/0/memory.node0_max", "r");
  if(!prof.profile_objmap.node0_max_file) {
    fprintf(stderr, "Failed to open node0_max file. Continuing anyway.\n");
  }
  
  retval = 0;
  if(prof.profile_objmap.node0_max_file) {
    line = NULL;
    size = 0;
    err = getline(&line, &size, prof.profile_objmap.node0_max_file);
    if(err < 0) {
      return 0;
    }
    retval = strtoull(line, NULL, 10);
  }
  fclose(prof.profile_objmap.node0_max_file);
  
  return retval;
}

size_t get_rss() {
  char *line, *value_ptr;
  size_t value, size, total;

  line = NULL;
  size = 0;
  total = 0;
  while (getline(&line, &size, prof.profile_objmap.smaps_file) > 0) {
    if (!strstr(line, RSS)) {
      free(line);
      line = NULL;
      size = 0;
      continue;
    }

    value_ptr = line + strlen(RSS);
    if (sscanf(value_ptr, "%zu kB", &value) < 1) {
      fprintf(stderr, "Failed to get an RSS from smaps. Aborting.\n");
      exit(1);
    }
    total += value;
  }
  fseek(prof.profile_objmap.smaps_file, 0, SEEK_SET);

  /* The return value here is in bytes */
  return total * 1024;
}

void profile_objmap_arena_init(per_arena_profile_objmap_info *info) {
  info->peak_present_bytes = 0;
  info->current_present_bytes = 0;
}

void profile_objmap_deinit() {
  objmap_close(&prof.profile_objmap.objmap);
  fclose(prof.profile_objmap.smaps_file);
}

void profile_objmap_init() {
  int status;
  
  status = objmap_open(&prof.profile_objmap.objmap);
  if (status < 0) {
    fprintf(stderr, "Failed to open the object map: %d. Aborting.\n", status);
    exit(1);
  }
  
  prof.profile_objmap.smaps_file = fopen("/proc/self/smaps", "r");
  if(!prof.profile_objmap.smaps_file) {
    fprintf(stderr, "Failed to open smaps file. Aborting.\n");
    exit(1);
  }
  
  prof.profile_objmap.pagesize = (size_t) sysconf(_SC_PAGESIZE);
  prof.profile_objmap.pid = getpid();
}

void *profile_objmap(void *a) {
  pthread_setcanceltype(PTHREAD_CANCEL_ASYNCHRONOUS, NULL);

  while(1) { }
}

/* Just copies the previous value */
void profile_objmap_skip_interval(int s) {
  get_profile_objmap_prof()->time = 0;
}

void profile_objmap_interval(int s) {
  size_t i, n, tot;
  uint64_t start, end;
  arena_info *arena;
  arena_profile *aprof;
  struct timespec start_time, end_time, actual;
  char entry_path_buff[4096];
  int status;
  struct proc_object_map_record_t record;
  sicm_device *dev;
  
  /* Time this interval */
  clock_gettime(CLOCK_MONOTONIC, &start_time);
  
  /* Grab the lock for the extents array */
  pthread_rwlock_rdlock(&tracker.extents_lock);
  
  extent_arr_for(tracker.extents, i) {
    arena = (arena_info *) tracker.extents->arr[i].arena;
    if(!arena) continue;
    aprof = get_arena_prof(arena->index);
    if(!aprof) continue;
    
    aprof->profile_objmap.current_present_bytes = 0;
  }
  
  /* TODO: Fix, because this is very machine-dependent */
  get_profile_objmap_prof()->upper_current = get_cgroup_node0_current();
  get_profile_objmap_prof()->lower_current = get_cgroup_node1_current();
  get_profile_objmap_prof()->cgroup_memory_current = get_cgroup_memory_current();
  get_profile_objmap_prof()->upper_max = get_cgroup_node0_max();
  
  /* Iterate over the chunks */
  get_profile_objmap_prof()->heap_bytes = 0;
  get_profile_objmap_prof()->lower_heap_bytes = 0;
  get_profile_objmap_prof()->upper_heap_bytes = 0;
  extent_arr_for(tracker.extents, i) {
    start = (uint64_t) tracker.extents->arr[i].start;
    end = (uint64_t) tracker.extents->arr[i].end;
    arena = (arena_info *) tracker.extents->arr[i].arena;
    if(!arena) continue;
    aprof = get_arena_prof(arena->index);
    if(!aprof) continue;

    snprintf(entry_path_buff, sizeof(entry_path_buff),
             "/proc/%d/object_map/%lx-%lx", prof.profile_objmap.pid, start, end);

    status = objmap_entry_read_record(entry_path_buff, &record);
    if (status == 0) {
      tot = record.n_resident_pages * prof.profile_objmap.pagesize;
      aprof->profile_objmap.current_present_bytes += tot;
      get_profile_objmap_prof()->heap_bytes += tot;
      
      /* Sometimes, `sh_create_extent` and `sh_delete_extent` will cause a deadlock if we don't
         release the lock here, because they grab the `sa->mutex` and then the `extents_lock`;
         meanwhile, this line without the unlocks would do the opposite. */
      pthread_rwlock_unlock(&tracker.extents_lock);
      dev = get_arena_device(arena->index);
      pthread_rwlock_rdlock(&tracker.extents_lock);
      
      if(dev) {
        if(dev == tracker.upper_device) {
          get_profile_objmap_prof()->upper_heap_bytes += tot;
        } else {
          get_profile_objmap_prof()->lower_heap_bytes += tot;
        }
      }
    }
  }
  
  pthread_rwlock_unlock(&tracker.extents_lock);
  
  clock_gettime(CLOCK_MONOTONIC, &end_time);
  timespec_diff(&start_time, &end_time, &actual);
  get_profile_objmap_prof()->time = actual.tv_sec + (((double) actual.tv_nsec) / 1000000000);
}

void profile_objmap_post_interval(arena_profile *info) {
  per_arena_profile_objmap_info *aprof;
  
  aprof = &(info->profile_objmap);
  /* Maintain the peak for this arena */
  if(aprof->current_present_bytes > aprof->peak_present_bytes) {
    aprof->peak_present_bytes = aprof->current_present_bytes;
  }
}
