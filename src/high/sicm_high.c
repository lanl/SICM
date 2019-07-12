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

/* Stores all machine devices and device
 * we should bind to by default */
static struct sicm_device_list device_list;
int num_numa_nodes;
deviceptr default_device;

/* Allocation site ID -> device */
tree(int, deviceptr) site_nodes;
/* Stores arenas associated with a device,
 * for the per-device arena layouts only. */
tree(deviceptr, int) device_arenas;

/* Keep track of all extents */
extent_arr *extents;
extent_arr *rss_extents; /* The extents that we want to get the RSS of */

/* Gets locked when we add a new extent */
pthread_rwlock_t extents_lock = PTHREAD_RWLOCK_INITIALIZER;

/* Keeps track of arenas */
arena_info **arenas;
static enum arena_layout layout;
static int max_arenas, arenas_per_thread, max_sites_per_arena;
int max_index;

/* Stores which arena an allocation site goes into. Only for
 * the `*_SITE_ARENAS` layouts, where there is an arena for
 * each allocation site.
 */
tree(int, int) site_arenas;
int arena_counter;

/* Gets locked when we add an arena */
pthread_mutex_t arena_lock = PTHREAD_MUTEX_INITIALIZER;

/* Associates a thread with an index (starting at 0) into the `arenas` array */
static pthread_key_t thread_key;
static int *thread_indices, *orig_thread_indices, *max_thread_indices, max_threads;
static int num_static_sites;

/* Passes an arena index to the extent hooks */
static int *pending_indices;

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

/* Converts an arena_layout to a string */
char *layout_str(enum arena_layout layout) {
  switch(layout) {
    case SHARED_ONE_ARENA:
      return "SHARED_ONE_ARENA";
    case EXCLUSIVE_ONE_ARENA:
      return "EXCLUSIVE_ONE_ARENA";
    case SHARED_DEVICE_ARENAS:
      return "SHARED_DEVICE_ARENAS";
    case EXCLUSIVE_DEVICE_ARENAS:
      return "EXCLUSIVE_DEVICE_ARENAS";
    case SHARED_SITE_ARENAS:
      return "SHARED_SITE_ARENAS";
    case EXCLUSIVE_SITE_ARENAS:
      return "EXCLUSIVE_SITE_ARENAS";
    case EXCLUSIVE_TWO_DEVICE_ARENAS:
      return "EXCLUSIVE_TWO_DEVICE_ARENAS";
    case EXCLUSIVE_FOUR_DEVICE_ARENAS:
      return "EXCLUSIVE_FOUR_DEVICE_ARENAS";
    default:
      break;
  }

  return "INVALID_LAYOUT";
}

/* Gets the SICM low-level device that corresponds to a NUMA node ID */
sicm_device *get_device_from_numa_node(int id) {
  struct sicm_device *retval, *device;
  int i;

  retval = NULL;
  /* Figure out which device the NUMA node corresponds to */
  device = device_list.devices;
  for(i = 0; i < device_list.count; i++) {
    /* If the device has a NUMA node, and if that node is the node we're
     * looking for.
     */
    if(sicm_numa_id(device) == id) {
      retval = device;
      break;
    }
    device++;
  }
  /* If we don't find an appropriate device, it stays NULL
   * so that no allocation sites will be bound to it
   */
  if(!retval) {
    fprintf(stderr, "Couldn't find an appropriate device for NUMA node %d.\n", id);
  }

  return retval;
}

int get_thread_index() {
  int *val;

  /* Get this thread's index */
  val = (int *) pthread_getspecific(thread_key);

  /* If nonexistent, increment the counter and set it */
  if(val == NULL) {
    if(thread_indices + 1 >= max_thread_indices) {
      fprintf(stderr, "Maximum number of threads reached. Aborting.\n");
      exit(1);
    }
    pthread_setspecific(thread_key, (void *) thread_indices);
    val = thread_indices;
    thread_indices++;
  }

  return *val;
}

/* Adds an arena to the `arenas` array. */
void sh_create_arena(int index, int id, sicm_device *device) {
  int i;

  if((arenas[index] != NULL) && (get_alloc_site(arenas[index], id) != -1)) {
    return;
  }

  /* Keep track of which arena we chose for this site */
  tree_insert(site_arenas, id, index);

  /* If we've already created this arena */
  if(arenas[index] != NULL) {

    /* Add the site to the arena */
    if(arenas[index]->num_alloc_sites == max_sites_per_arena) {
      fprintf(stderr, "Sites: ");
      for(i = 0; i < arenas[index]->num_alloc_sites; i++) {
        fprintf(stderr, "%d ", arenas[index]->alloc_sites[i]);
      }
      fprintf(stderr, "\n");
      fprintf(stderr, "Tried to allocate %d sites into an arena. Increase SH_MAX_SITES_PER_ARENA.\n", max_sites_per_arena + 1);
      exit(1);
    }
    arenas[index]->alloc_sites[arenas[index]->num_alloc_sites] = id;
    arenas[index]->num_alloc_sites++;

    return;
  }

  /* Put an upper bound on the indices that need to be searched */
  if(index > max_index) {
    max_index = index;
  }

  if(!device) {
    device = default_device;
  }

  /* Create the arena if it doesn't exist */
  arenas[index] = calloc(1, sizeof(arena_info));
  arenas[index]->index = index;
  arenas[index]->accesses = 0;
  arenas[index]->acc_per_sample = 0.0;
  arenas[index]->alloc_sites = malloc(sizeof(int) * max_sites_per_arena);
  arenas[index]->alloc_sites[0] = id;
  arenas[index]->num_alloc_sites = 1;
  arenas[index]->rss = 0;
  arenas[index]->peak_rss = 0;
  arenas[index]->avg_rss = 0;

  /* Need to construct a sicm_device_list of one device */
  sicm_device_list dl;
  dl.count = 1;
  dl.devices = device;
  arenas[index]->arena = sicm_arena_create(0, SICM_ALLOC_RELAXED, &dl);
}

/* Adds an extent to the `extents` array. */
void sh_create_extent(void *start, void *end) {
  int thread_index, arena_index;

  /* Get this thread's current arena index from `pending_indices` */
  thread_index = get_thread_index();
  arena_index = pending_indices[thread_index];

  /* A extent allocation is happening without an sh_alloc... */
  if(arena_index == -1) {
    fprintf(stderr, "Unknown extent allocation. Aborting.\n");
    exit(1);
  }

  if(should_profile_rss && should_profile_one && (get_alloc_site(arenas[arena_index], profile_one_site) != -1)) {
    /* If we're profiling RSS and this is the site that we're isolating */
    extent_arr_insert(rss_extents, start, end, arenas[arena_index]);
  }

  if(pthread_rwlock_wrlock(&extents_lock) != 0) {
    fprintf(stderr, "Failed to acquire read/write lock. Aborting.\n");
    exit(1);
  }
  extent_arr_insert(extents, start, end, arenas[arena_index]);
  if(pthread_rwlock_unlock(&extents_lock) != 0) {
    fprintf(stderr, "Failed to unlock read/write lock. Aborting.\n");
    exit(1);
  }
}

int get_site_arena(int id) {
  tree_it(int, int) it;
  int ret;

  it = tree_lookup(site_arenas, id);
  if(tree_it_good(it)) {
    /* We've already got an arena for this site, use it */
    ret = tree_it_val(it);
  } else {
    /* We need to create an arena for this site. Grab the next
     * available arena and increment.
     */
    ret = __sync_fetch_and_add(&arena_counter, 1);
  }

  return ret;
}

/* Gets the device that this site should go onto from the site_nodes tree */
sicm_device_list *get_site_device(int id) {
  deviceptr device;
  tree_it(int, deviceptr) it;

  it = tree_lookup(site_nodes, id);
  if(tree_it_good(it)) {
    /* This site was found in the guidance file.  Use its device pointer to
     * find if this device has already got an arena.
     */
    device = tree_it_val(it);
  } else {
    /* Site's not in the guidance file. Use the default device. */
    device = default_device;
  }
  if(id == profile_one_site) {
    device = profile_one_device;
  }

  return device;
}

/* Chooses an arena for the per-device arena layouts. */
int get_device_arena(int id, deviceptr *device) {
  tree_it(deviceptr, int) devit;
  int ret;

  *device = get_site_device(id);
  devit = tree_lookup(device_arenas, *device);
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
    ret = max_index + 1;
    tree_insert(device_arenas, *device, ret);
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
  switch(layout) {
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
      ret = (thread_index * arenas_per_thread) + ret;
      break;
    case SHARED_SITE_ARENAS:
      ret = get_site_arena(id);
      device = get_site_device(id);
      break;
    case EXCLUSIVE_SITE_ARENAS:
      ret = (thread_index * arenas_per_thread) + id;
      break;
    case EXCLUSIVE_TWO_DEVICE_ARENAS:
      ret = get_device_arena(id, &device);
      ret = (thread_index * arenas_per_thread) + ret;
    case EXCLUSIVE_FOUR_DEVICE_ARENAS:
      ret = get_device_arena(id, &device);
      ret = (thread_index * arenas_per_thread) + ret;
      break;
    default:
      fprintf(stderr, "Invalid arena layout. Aborting.\n");
      exit(1);
      break;
  };

  if(ret > max_arenas) {
    /* Fit the index to the maximum number of arenas */
    ret = ret % max_arenas;
  }

  pthread_mutex_lock(&arena_lock);
  pending_indices[thread_index] = ret;
  sh_create_arena(ret, id, device);
  pthread_mutex_unlock(&arena_lock);

  return ret;
}

void* sh_realloc(int id, void *ptr, size_t sz) {
  int   index;
  void *ret;

  if(layout == INVALID_LAYOUT) {
    ret = realloc(ptr, sz);
  } else {
    index = get_arena_index(id);
    ret = sicm_arena_realloc(arenas[index]->arena, ptr, sz);
  }

  if (should_run_rdspy) {
    sh_rdspy_realloc(ptr, ret, sz, id);
  }

  return ret;
}

/* Accepts an allocation site ID and a size, does the allocation */
void* sh_alloc(int id, size_t sz) {
  int index;
  void *ret;

  if((layout == INVALID_LAYOUT) || !sz) {
    ret = je_malloc(sz);
  } else {
    index = get_arena_index(id);
    ret = sicm_arena_alloc(arenas[index]->arena, sz);
  }

  if (should_run_rdspy) {
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

  if((layout == INVALID_LAYOUT) || !sz) {
    ret = je_aligned_alloc(alignment, sz);
  } else {
    index = get_arena_index(id);
    ret = sicm_arena_alloc_aligned(arenas[index]->arena, sz, alignment);
  }

  if (should_run_rdspy) {
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
  if (should_run_rdspy) {
      sh_rdspy_free(ptr);
  }

  if(!ptr) {
    return;
  }

  if(layout == INVALID_LAYOUT) {
    je_free(ptr);
  } else {
    sicm_free(ptr);
  }
}

