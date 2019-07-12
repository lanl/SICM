#include <fcntl.h>
#include <numa.h>
#include <numaif.h>
#include <stdlib.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <stdio.h>
#include <errno.h>
#include <string.h>
#include <inttypes.h>
#include <pthread.h>
#include <jemalloc/jemalloc.h>

#include "sicm_high.h"
#include "sicm_low.h"
#include "sicm_impl.h"
#include "sicm_profile.h"
#include "sicm_rdspy.h"

/* Takes a string as input and outputs which arena layout it is */
enum arena_layout parse_layout(char *env) {
	size_t max_chars;

	max_chars = 32;

	if(strncmp(env, "SHARED_ONE_ARENA", max_chars) == 0) {
		return SHARED_ONE_ARENA;
	} else if(strncmp(env, "EXCLUSIVE_ONE_ARENA", max_chars) == 0) {
		return EXCLUSIVE_ONE_ARENA;
	} else if(strncmp(env, "SHARED_DEVICE_ARENAS", max_chars) == 0) {
		return SHARED_DEVICE_ARENAS;
	} else if(strncmp(env, "EXCLUSIVE_DEVICE_ARENAS", max_chars) == 0) {
		return EXCLUSIVE_DEVICE_ARENAS;
	} else if(strncmp(env, "SHARED_SITE_ARENAS", max_chars) == 0) {
		return SHARED_SITE_ARENAS;
	} else if(strncmp(env, "EXCLUSIVE_SITE_ARENAS", max_chars) == 0) {
		return EXCLUSIVE_SITE_ARENAS;
	} else if(strncmp(env, "EXCLUSIVE_TWO_DEVICE_ARENAS", max_chars) == 0) {
		return EXCLUSIVE_TWO_DEVICE_ARENAS;
	} else if(strncmp(env, "EXCLUSIVE_FOUR_DEVICE_ARENAS", max_chars) == 0) {
		return EXCLUSIVE_FOUR_DEVICE_ARENAS;
	}

  return INVALID_LAYOUT;
}

/* Returns the index of an allocation site in an arena,
 * -1 if it's not there */
int get_alloc_site(arena_info *arena, int id) {
  int i;
  for(i = 0; i < arena->num_alloc_sites; i++) {
    if(arena->alloc_sites[i] == id) {
      return i;
    }
  }

  return -1;
}


int get_thread_index() {
  int *val;

  /* Get this thread's index */
  val = (int *) pthread_getspecific(tracker.thread_key);

  /* If nonexistent, increment the counter and set it */
  if(val == NULL) {
    if(tracker.thread_indices + 1 >= tracker.max_thread_indices) {
      fprintf(stderr, "Maximum number of threads reached. Aborting.\n");
      exit(1);
    }
    pthread_setspecific(tracker.thread_key, (void *) tracker.thread_indices);
    val = tracker.thread_indices;
    tracker.thread_indices++;
  }

  return *val;
}

/* Adds an arena to the `arenas` array. */
void sh_create_arena(int index, int id, sicm_device *device) {
  int i;

  if((tracker.arenas[index] != NULL) && (get_alloc_site(tracker.arenas[index], id) != -1)) {
    return;
  }

  /* Keep track of which arena we chose for this site */
  tree_insert(tracker.site_arenas, id, index);

  /* If we've already created this arena */
  if(tracker.arenas[index] != NULL) {

    /* Add the site to the arena */
    if(tracker.arenas[index]->num_alloc_sites == tracker.max_sites_per_arena) {
      fprintf(stderr, "Sites: ");
      for(i = 0; i < tracker.arenas[index]->num_alloc_sites; i++) {
        fprintf(stderr, "%d ", tracker.arenas[index]->alloc_sites[i]);
      }
      fprintf(stderr, "\n");
      fprintf(stderr, "Tried to allocate %d sites into an arena. Increase SH_MAX_SITES_PER_ARENA.\n", tracker.max_sites_per_arena + 1);
      exit(1);
    }
    tracker.arenas[index]->alloc_sites[tracker.arenas[index]->num_alloc_sites] = id;
    tracker.arenas[index]->num_alloc_sites++;

    return;
  }

  /* Put an upper bound on the indices that need to be searched */
  if(index > tracker.max_index) {
    tracker.max_index = index;
  }

  if(!device) {
    device = tracker.default_device;
  }

  /* Create the arena if it doesn't exist */
  tracker.arenas[index] = calloc(1, sizeof(arena_info));
  tracker.arenas[index]->index = index;
  tracker.arenas[index]->accesses = 0;
  tracker.arenas[index]->acc_per_sample = 0.0;
  tracker.arenas[index]->alloc_sites = malloc(sizeof(int) * tracker.max_sites_per_arena);
  tracker.arenas[index]->alloc_sites[0] = id;
  tracker.arenas[index]->num_alloc_sites = 1;
  tracker.arenas[index]->rss = 0;
  tracker.arenas[index]->peak_rss = 0;
  tracker.arenas[index]->avg_rss = 0;

  /* Need to construct a sicm_device_list of one device */
  sicm_device_list dl;
  dl.count = 1;
  dl.devices[0] = device;
  printf("device: %p\n", device);
  printf("dl: %p\n", &dl);
  printf("dl.devices: %p\n", dl.devices);
  tracker.arenas[index]->arena = sicm_arena_create(0, SICM_ALLOC_RELAXED, &dl);
}

/* Adds an extent to the `extents` array. */
void sh_create_extent(void *start, void *end) {
  int thread_index, arena_index;

  /* Get this thread's current arena index from `pending_indices` */
  thread_index = get_thread_index();
  arena_index = tracker.pending_indices[thread_index];

  /* A extent allocation is happening without an sh_alloc... */
  if(arena_index == -1) {
    fprintf(stderr, "Unknown extent allocation. Aborting.\n");
    exit(1);
  }

  if(profopts.should_profile_rss && profopts.should_profile_one && (get_alloc_site(tracker.arenas[arena_index], profopts.profile_one_site) != -1)) {
    /* If we're profiling RSS and this is the site that we're isolating */
    extent_arr_insert(tracker.rss_extents, start, end, tracker.arenas[arena_index]);
  }

  if(pthread_rwlock_wrlock(&tracker.extents_lock) != 0) {
    fprintf(stderr, "Failed to acquire read/write lock. Aborting.\n");
    exit(1);
  }
  extent_arr_insert(tracker.extents, start, end, tracker.arenas[arena_index]);
  if(pthread_rwlock_unlock(&tracker.extents_lock) != 0) {
    fprintf(stderr, "Failed to unlock read/write lock. Aborting.\n");
    exit(1);
  }
}

int get_site_arena(int id) {
  tree_it(int, int) it;
  int ret;

  it = tree_lookup(tracker.site_arenas, id);
  if(tree_it_good(it)) {
    /* We've already got an arena for this site, use it */
    ret = tree_it_val(it);
  } else {
    /* We need to create an arena for this site. Grab the next
     * available arena and increment.
     */
    ret = __sync_fetch_and_add(&tracker.arena_counter, 1);
  }

  return ret;
}

/* Gets the device that this site should go onto from the site_nodes tree */
sicm_device_list *get_site_device(int id) {
  deviceptr device;
  tree_it(int, deviceptr) it;

  it = tree_lookup(tracker.site_nodes, id);
  if(tree_it_good(it)) {
    /* This site was found in the guidance file.  Use its device pointer to
     * find if this device has already got an arena.
     */
    device = tree_it_val(it);
  } else {
    /* Site's not in the guidance file. Use the default device. */
    device = tracker.default_device;
  }
  if(id == profopts.profile_one_site) {
    device = profopts.profile_one_device;
  }

  return device;
}

/* Chooses an arena for the per-device arena layouts. */
int get_device_arena(int id, deviceptr *device) {
  tree_it(deviceptr, int) devit;
  int ret;

  *device = get_site_device(id);
  devit = tree_lookup(tracker.device_arenas, *device);
  if(tree_it_good(devit)) {
    /* This device already has an arena associated with it. Return the
     * index of that arena.
     */
    ret = tree_it_val(devit);
  } else {
    /* Choose an arena index for this device.  We're going to assume here
     * that we never get a device that didn't exist on initialization.
     * Remember our choice.
     */
    ret = tracker.max_index + 1;
    tree_insert(tracker.device_arenas, *device, ret);
  }

  return ret;
}

/* Gets the index that the allocation site should go into */
int get_arena_index(int id) {
  int ret, thread_index;
  deviceptr device;
  tree_it(int, int) it;

  thread_index = get_thread_index();

  ret = 0;
  device = NULL;
  switch(tracker.layout) {
    case SHARED_ONE_ARENA:
      ret = 0;
      break;
    case EXCLUSIVE_ONE_ARENA:
      ret = thread_index + 1;
      break;
    case SHARED_DEVICE_ARENAS:
      ret = get_device_arena(id, &device);
      break;
    case EXCLUSIVE_DEVICE_ARENAS:
      /* Same as SHARED_DEVICE_ARENAS, except per thread */
      ret = get_device_arena(id, &device);
      ret = (thread_index * tracker.arenas_per_thread) + ret;
      break;
    case SHARED_SITE_ARENAS:
      ret = get_site_arena(id);
      device = get_site_device(id);
      break;
    case EXCLUSIVE_SITE_ARENAS:
      ret = (thread_index * tracker.arenas_per_thread) + id;
      break;
    case EXCLUSIVE_TWO_DEVICE_ARENAS:
      ret = get_device_arena(id, &device);
      ret = (thread_index * tracker.arenas_per_thread) + ret;
    case EXCLUSIVE_FOUR_DEVICE_ARENAS:
      ret = get_device_arena(id, &device);
      ret = (thread_index * tracker.arenas_per_thread) + ret;
      break;
    default:
      fprintf(stderr, "Invalid arena layout. Aborting.\n");
      exit(1);
      break;
  };

  if(ret > tracker.max_arenas) {
    /* Fit the index to the maximum number of arenas */
    ret = ret % tracker.max_arenas;
  }

  pthread_mutex_lock(&tracker.arena_lock);
  tracker.pending_indices[thread_index] = ret;
  sh_create_arena(ret, id, device);
  pthread_mutex_unlock(&tracker.arena_lock);

  return ret;
}

void* sh_realloc(int id, void *ptr, size_t sz) {
  int   index;
  void *ret;

  if(tracker.layout == INVALID_LAYOUT) {
    ret = realloc(ptr, sz);
  } else {
    index = get_arena_index(id);
    ret = sicm_arena_realloc(tracker.arenas[index]->arena, ptr, sz);
  }

  if (profopts.should_run_rdspy) {
    sh_rdspy_realloc(ptr, ret, sz, id);
  }

  return ret;
}

/* Accepts an allocation site ID and a size, does the allocation */
void* sh_alloc(int id, size_t sz) {
  int index;
  void *ret;

  if((tracker.layout == INVALID_LAYOUT) || !sz) {
    ret = je_malloc(sz);
  } else {
    index = get_arena_index(id);
    ret = sicm_arena_alloc(tracker.arenas[index]->arena, sz);
  }

  if (profopts.should_run_rdspy) {
    sh_rdspy_alloc(ret, sz, id);
  }
  
  return ret;
}

/* Accepts an allocation site ID and a size, does the allocation */
void* sh_aligned_alloc(int id, size_t alignment, size_t sz) {
  int index;
  void *ret;

  if(!sz) {
    return NULL;
  }

  if((tracker.layout == INVALID_LAYOUT) || !sz) {
    ret = je_aligned_alloc(alignment, sz);
  } else {
    index = get_arena_index(id);
    ret = sicm_arena_alloc_aligned(tracker.arenas[index]->arena, sz, alignment);
  }

  if (profopts.should_run_rdspy) {
    sh_rdspy_alloc(ret, sz, id);
  }
  
  return ret;
}

int sh_posix_memalign(int id, void **ptr, size_t alignment, size_t sz) {
  *ptr = sh_aligned_alloc(id, alignment, sz);
  return 0;
}

void *sh_memalign(int id, size_t alignment, size_t sz) {
  return sh_aligned_alloc(id, alignment, sz);
}

void* sh_calloc(int id, size_t num, size_t sz) {
  void *ptr;
  size_t i;

  ptr = sh_alloc(id, num * sz);
  memset(ptr, 0, num * sz);
  return ptr;
}

void sh_free(void* ptr) {
  if (profopts.should_run_rdspy) {
      sh_rdspy_free(ptr);
  }

  if(!ptr) {
    return;
  }

  if(tracker.layout == INVALID_LAYOUT) {
    je_free(ptr);
  } else {
    sicm_free(ptr);
  }
}

