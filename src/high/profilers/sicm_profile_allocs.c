#include <sys/stat.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <unistd.h>
#include <sys/syscall.h>
#include <errno.h>
#include <sys/types.h>
#include "sicm_runtime.h"
#include "sicm_profilers.h"
#include "sicm_profile.h"

void profile_allocs_arena_init(profile_allocs_info *);
void profile_allocs_deinit();
void profile_allocs_init();
void *profile_allocs(void *);
void profile_allocs_interval(int);
void profile_allocs_skip_interval(int);
void profile_allocs_post_interval(profile_info *);

void profile_allocs_arena_init(profile_allocs_info *info) {
  info->peak = 0;
  info->intervals = NULL;
  info->tmp_accumulator = 0;
}

void *profile_allocs(void *a) {
  pthread_setcanceltype(PTHREAD_CANCEL_ASYNCHRONOUS, NULL);

  while(1) { }
}

void profile_allocs_interval(int s) {
  arena_info *arena;
  profile_info *profinfo;
  size_t i;

  /* Iterate over the arenas and set their size to the tmp_accumulator */
  for(i = 0; i <= tracker.max_index; i++) {
    arena = tracker.arenas[i];
    profinfo = prof.info[i];
    if((!arena) || (!profinfo) || (!profinfo->num_intervals)) continue;

    profinfo->profile_allocs.tmp_accumulator = arena->size;
  }

  end_interval();
}

void profile_allocs_init() {
  tracker.profile_allocs_map = tree_make(addr_t, alloc_info_ptr);
  pthread_rwlock_init(&tracker.profile_allocs_map_lock, NULL);
}

void profile_allocs_deinit() {
}

void profile_allocs_post_interval(profile_info *info) {
  profile_allocs_info *profinfo;

  profinfo = &(info->profile_allocs);

  /* Maintain peak */
  if(profinfo->tmp_accumulator > profinfo->peak) {
    profinfo->peak = profinfo->tmp_accumulator;
  }

  /* Store this interval */
  profinfo->intervals = 
    (size_t *)orig_realloc(profinfo->intervals, info->num_intervals * sizeof(size_t));
  profinfo->intervals[info->num_intervals - 1] = profinfo->tmp_accumulator;
}

void profile_allocs_skip_interval(int s) {
  /* TODO */
}
