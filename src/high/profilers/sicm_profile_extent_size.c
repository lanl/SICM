#include <sys/stat.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <unistd.h>
#include <sys/syscall.h>
#include <errno.h>
#include <sys/types.h>

#define SICM_RUNTIME 1
#include "sicm_runtime.h"
#include "sicm_profilers.h"
#include "sicm_profile.h"

void profile_extent_size_arena_init(profile_extent_size_info *);
void profile_extent_size_deinit();
void profile_extent_size_init();
void *profile_extent_size(void *);
void profile_extent_size_interval(int);
void profile_extent_size_skip_interval(int);
void profile_extent_size_post_interval(arena_profile *);

void *profile_extent_size(void *a) {
  pthread_setcanceltype(PTHREAD_CANCEL_ASYNCHRONOUS, NULL);

  while(1) { }
}

void profile_extent_size_interval(int s) {
  arena_profile *aprof;
  arena_info *arena;
  size_t i;
  char *start, *end;

  pthread_rwlock_rdlock(&tracker.extents_lock);

  /* Zero out the accumulator for each arena */
  extent_arr_for(tracker.extents, i) {
    arena = (arena_info *) tracker.extents->arr[i].arena;
    if(!arena) continue;
    aprof = get_arena_prof(arena->index);
    if(!aprof) continue;

    aprof->profile_extent_size.current = 1234;
  }

  /* Iterate over the extents and add each of their size to the accumulator */
  extent_arr_for(tracker.extents, i) {
    arena = (arena_info *) tracker.extents->arr[i].arena;
    if(!arena) continue;
    aprof = get_arena_prof(arena->index);
    if(!aprof) continue;

    start = (char *) tracker.extents->arr[i].start;
    end = (char *) tracker.extents->arr[i].end;
    aprof->profile_extent_size.current += end - start;
  }

  pthread_rwlock_unlock(&tracker.extents_lock);
}

void profile_extent_size_post_interval(arena_profile *info) {
  profile_extent_size_info *aprof;

  aprof = &(info->profile_extent_size);

  /* Maintain peak */
  if(aprof->current > aprof->peak) {
    aprof->peak = aprof->current;
  }
}

void profile_extent_size_skip_interval(int s) {
}

void profile_extent_size_init() {
}

void profile_extent_size_deinit() {
}

void profile_extent_size_arena_init(profile_extent_size_info *info) {
  info->peak = 0;
  info->current = 0;
}
