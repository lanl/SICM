#define _GNU_SOURCE
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

/* Required for dlsym */
#include <dlfcn.h>

#include "sicm_runtime.h"
#include "sicm_rdspy.h"

static void *(*orig_malloc_ptr)(size_t) = NULL;
static void *(*orig_calloc_ptr)(size_t, size_t) = NULL;
static void *(*orig_realloc_ptr)(void *, size_t) = NULL;
static void (*orig_free_ptr)(void *) = NULL;

void *__attribute__ ((noinline)) orig_malloc(size_t size) {
  if(!orig_malloc_ptr) {
    orig_malloc_ptr = dlsym(RTLD_NEXT, "malloc");
  }
  return (*orig_malloc_ptr)(size);
}

void *__attribute__ ((noinline)) orig_calloc(size_t num, size_t size) {
  if(!orig_calloc_ptr) {
    orig_calloc_ptr = dlsym(RTLD_NEXT, "calloc");
  }
  return (*orig_calloc_ptr)(num, size);
}

void *__attribute__ ((noinline)) orig_realloc(void *ptr, size_t size) {
  if(!orig_realloc_ptr) {
    orig_realloc_ptr = dlsym(RTLD_NEXT, "realloc");
  }
  return (*orig_realloc_ptr)(ptr, size);
}

void __attribute__ ((noinline)) orig_free(void *ptr) {
  if(!orig_free_ptr) {
    orig_free_ptr = dlsym(RTLD_NEXT, "free");
  }
  (*orig_free_ptr)(ptr);
  return;
}

void profile_allocs_alloc(void *ptr, size_t size, int index) {
  alloc_info_ptr aip;

  /* Add to this arena's size */
  tracker.arenas[index]->size += size;

  /* Construct the alloc_info struct */
  aip = (alloc_info_ptr) orig_malloc(sizeof(alloc_info));
  aip->size = size;
  aip->index = index;

  /* Add it to the map */
  pthread_rwlock_wrlock(&tracker.profile_allocs_map_lock);
  tree_insert(tracker.profile_allocs_map, ptr, aip);
  pthread_rwlock_unlock(&tracker.profile_allocs_map_lock);
}

void profile_allocs_realloc(void *ptr, size_t size, int index) {
  alloc_info_ptr aip;

  /* Replace this arena's size */
  tracker.arenas[index]->size = size;

  /* Construct the struct that logs this allocation's arena
   * index and size of the allocation */
  aip = (alloc_info_ptr) orig_malloc(sizeof(alloc_info));
  aip->size = size;
  aip->index = index;

  /* Replace in the map */
  pthread_rwlock_wrlock(&tracker.profile_allocs_map_lock);
  tree_delete(tracker.profile_allocs_map, ptr);
  tree_insert(tracker.profile_allocs_map, ptr, aip);
  pthread_rwlock_unlock(&tracker.profile_allocs_map_lock);
}

void profile_allocs_free(void *ptr) {
  tree_it(addr_t, alloc_info_ptr) it;
  alloc_info_ptr aip;

  /* Look up the pointer in the map */
  pthread_rwlock_wrlock(&tracker.profile_allocs_map_lock);
  it = tree_lookup(tracker.profile_allocs_map, ptr);
  if(tree_it_good(it)) {
    aip = tree_it_val(it);
  } else {
    fprintf(stderr, "WARNING: Couldn't find a pointer to free in the map of allocations.\n");
    pthread_rwlock_unlock(&tracker.profile_allocs_map_lock);
    return;
  }

  /* Subtract from the size of the arena */
  tracker.arenas[aip->index]->size -= aip->size;

  /* Remove the allocation from the map */
  tree_delete(tracker.profile_allocs_map, ptr);
  pthread_rwlock_unlock(&tracker.profile_allocs_map_lock);
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
    pthread_mutex_lock(&tracker.thread_lock);
    if(tracker.thread_indices + 1 >= tracker.max_thread_indices) {
      fprintf(stderr, "Maximum number of threads reached. Aborting!\n");
      exit(1);
    }
    pthread_setspecific(tracker.thread_key, (void *) tracker.thread_indices);
    val = tracker.thread_indices;
    tracker.thread_indices++;
    pthread_mutex_unlock(&tracker.thread_lock);
  }

  return *val;
}

/* Adds an arena to the `arenas` array. */
void sh_create_arena(int index, int id, sicm_device *device) {
  size_t i;
  arena_info *arena;

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
  arena = orig_calloc(1, sizeof(arena_info));
  arena->index = index;
  arena->alloc_sites = orig_malloc(sizeof(int) * tracker.max_sites_per_arena);
  arena->alloc_sites[0] = id;
  arena->num_alloc_sites = 1;
  if(profopts.should_profile) {
    arena->info = create_profile_arena(index);
  }

  /* Need to construct a sicm_device_list of one device */
  sicm_device_list dl;
  dl.count = 1;
  dl.devices = orig_malloc(sizeof(sicm_device *) * 1);
  dl.devices[0] = device;
  arena->arena = sicm_arena_create(0, SICM_ALLOC_RELAXED, &dl);
  orig_free(dl.devices);

  /* Now add the arena to the array of arenas */
  tracker.arenas[index] = arena;
}

/* Adds an extent to the `extents` array. */
void sh_create_extent(sarena *arena, void *start, void *end) {
  int thread_index, arena_index;

  /* Get this thread's current arena index from `pending_indices` */
  thread_index = get_thread_index();
  arena_index = tracker.pending_indices[thread_index];

  /* A extent allocation is happening without an sh_alloc... */
  if(arena_index == -1) {
    fprintf(stderr, "Unknown extent allocation to thread_index %d. Aborting.\n", thread_index);
    exit(1);
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

void sh_delete_extent(sarena *arena, void *start, void *end) {
  extent_arr_delete(tracker.extents, start);
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
    printf("Site %d goes to node %d\n", id, device->node);
  } else {
    /* Site's not in the guidance file. Use the default device. */
    device = tracker.default_device;
  }
  if(profopts.should_profile_one && (id == profopts.profile_one_site)) {
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
    fprintf(stderr, "WARNING: Overflowing maximum arenas.\n");
    ret = ret % tracker.max_arenas;
  }

  printf("Site %d is going to arena %d.\n", id, ret);

  pthread_mutex_lock(&tracker.arena_lock);
  tracker.pending_indices[thread_index] = ret;
  sh_create_arena(ret, id, device);
  pthread_mutex_unlock(&tracker.arena_lock);

  return ret;
}

void* sh_realloc(int id, void *ptr, size_t sz) {
  int   index;
  void *ret;
  alloc_info_ptr aip;

  if((tracker.layout == INVALID_LAYOUT) || (id == 0) || (!sh_initialized)) {
    ret = je_realloc(ptr, sz);
  } else {
    index = get_arena_index(id);
    ret = sicm_arena_realloc(tracker.arenas[index]->arena, ptr, sz);

    if(profopts.should_profile_allocs) {
      profile_allocs_realloc(ptr, sz, index);
    }
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
  alloc_info_ptr aip;

  printf("Calling sh_alloc(%d, %zu) with init %d\n", id, sz, sh_initialized);

  if((tracker.layout == INVALID_LAYOUT) || !sz || (id == 0) || (!sh_initialized)) {
    ret = je_malloc(sz);
  } else {
    index = get_arena_index(id);
    ret = sicm_arena_alloc(tracker.arenas[index]->arena, sz);

    if(profopts.should_profile_allocs) {
      profile_allocs_alloc(ret, sz, index);
    }
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

  if(!sh_initialized) {
    return je_aligned_alloc(alignment, sz);
  }

  if(!sz) {
    return NULL;
  }

  if(tracker.layout == INVALID_LAYOUT) {
    ret = je_aligned_alloc(alignment, sz);
  } else {
    index = get_arena_index(id);
    if(profopts.should_profile_allocs) {
      tracker.arenas[index]->size += sz;
    }
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
  if(!ptr) {
    return;
  }

  if(!sh_initialized) {
    je_free(ptr);
    return;
  }

  if (profopts.should_run_rdspy) {
      sh_rdspy_free(ptr);
  }

  if(profopts.should_profile_allocs) {
    profile_allocs_free(ptr);
  }

  if(tracker.layout == INVALID_LAYOUT) {
    je_free(ptr);
  } else {
    sicm_free(ptr);
  }
}
