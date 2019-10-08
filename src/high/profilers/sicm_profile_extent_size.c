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

void profile_extent_size_arena_init(profile_extent_size_info *);
void profile_extent_size_deinit();
void profile_extent_size_init();
void *profile_extent_size(void *);
void profile_extent_size_interval(int);
void profile_extent_size_skip_interval(int);
void profile_extent_size_post_interval(profile_info *);

void *profile_extent_size(void *a) {
  pthread_setcanceltype(PTHREAD_CANCEL_ASYNCHRONOUS, NULL);

  while(1) { }
}

void profile_extent_size_interval(int s) {
  profile_info *profinfo;
  arena_info *arena;
  size_t i;
  char *start, *end;

  pthread_rwlock_rdlock(&tracker.extents_lock);
  
  /* Zero out the accumulator for each arena */
  extent_arr_for(tracker.extents, i) {
    arena = (arena_info *) tracker.extents->arr[i].arena;
    if(!arena) continue;
    profinfo = (profile_info *) prof.info[arena->index];
    if(!profinfo) continue;

    profinfo->profile_extent_size.tmp_accumulator = 0;
  }

  /* Iterate over the extents and add each of their size to the accumulator */
  extent_arr_for(tracker.extents, i) {
    arena = (arena_info *) tracker.extents->arr[i].arena;
    if(!arena) continue;
    profinfo = (profile_info *) prof.info[arena->index];
    if((!profinfo) || (!profinfo->num_intervals)) continue;

    start = (char *) tracker.extents->arr[i].start;
    end = (char *) tracker.extents->arr[i].end;
    profinfo->profile_extent_size.tmp_accumulator += end - start;
  }

  pthread_rwlock_unlock(&tracker.extents_lock);

  end_interval();
}

void profile_extent_size_post_interval(profile_info *info) {
  profile_extent_size_info *profinfo;

  profinfo = &(info->profile_extent_size);

  /* Maintain peak */
  if(profinfo->tmp_accumulator > profinfo->peak) {
    profinfo->peak = profinfo->tmp_accumulator;
  }

  /* Store this interval */
  profinfo->intervals = 
    (size_t *)orig_realloc(profinfo->intervals, info->num_intervals * sizeof(size_t));
  profinfo->intervals[info->num_intervals - 1] = profinfo->tmp_accumulator;
}

/* Just copies previous values along */
void profile_extent_size_skip_interval(int s) {
  profile_info *profinfo;
  arena_info *arena;
  size_t i;

  arena_arr_for(i) {
    prof_check_good(arena, profinfo, i);

    if(profinfo->num_intervals == 1) {
      profinfo->profile_extent_size.tmp_accumulator = 0;
    } else {
      profinfo->profile_extent_size.tmp_accumulator = profinfo->profile_extent_size.intervals[profinfo->num_intervals - 2];
    }
  }

  end_interval();
}

void profile_extent_size_init() {
}

void profile_extent_size_deinit() {
}

void profile_extent_size_arena_init(profile_extent_size_info *info) {
  info->peak = 0;
  info->intervals = NULL;
  info->tmp_accumulator = 0;
}
