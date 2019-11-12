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

void profile_allocs_arena_init(profile_allocs_info *);
void profile_allocs_deinit();
void profile_allocs_init();
void *profile_allocs(void *);
void profile_allocs_interval(int);
void profile_allocs_skip_interval(int);
void profile_allocs_post_interval(arena_profile *);

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
  arena_profile *aprof;
  size_t i;

  /* Iterate over the arenas and set their size to the tmp_accumulator */
  arena_arr_for(i) {
    prof_check_good(arena, aprof, i);
    aprof->profile_allocs.tmp_accumulator = arena->size;
  }

  end_interval();
}

void profile_allocs_init() {
  tracker.profile_allocs_map = tree_make(addr_t, alloc_info_ptr);
  pthread_rwlock_init(&tracker.profile_allocs_map_lock, NULL);
}

void profile_allocs_deinit() {
}

void profile_allocs_post_interval(arena_profile *aprof) {
  profile_allocs_info *aprof_allocs;

  aprof_allocs = &(aprof->profile_allocs);

  /* Maintain peak */
  if(aprof_allocs->tmp_accumulator > aprof_allocs->peak) {
    aprof_allocs->peak = aprof_allocs->tmp_accumulator;
  }

  /* Store this interval */
  aprof_allocs->intervals =
    (size_t *)orig_realloc(aprof_allocs->intervals, aprof->num_intervals * sizeof(size_t));
  aprof->intervals[info->num_intervals - 1] = aprof_allocs->tmp_accumulator;
}

void profile_allocs_skip_interval(int s) {
  arena_info *arena;
  arena_profile *aprof;
  size_t i;

  arena_arr_for(i) {
    prof_check_good(arena, aprof, i);

    if(aprof->num_intervals == 1) {
      aprof->profile_allocs.tmp_accumulator = 0;
    } else {
      aprof->profile_allocs.tmp_accumulator = aprof->profile_allocs.intervals[aprof->num_intervals - 2];
    }
  }

  end_interval();
}
