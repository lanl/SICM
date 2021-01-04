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

#define SICM_RUNTIME 1
#include "sicm_runtime.h"
#include "sicm_profile.h"
#include "sicm_rdspy.h"

/* Supported by every compiler that I can find, and Clang 2.0+ */
__thread int thread_index = -1;
__thread int pending_index = -1;

atomic_int sh_initialized = 0;
void *(*orig_malloc_ptr)(size_t);
void *(*orig_valloc_ptr)(size_t);
void *(*orig_calloc_ptr)(size_t, size_t);
void *(*orig_realloc_ptr)(void *, size_t);
void (*orig_free_ptr)(void *);

void print_sizet(size_t val, const char *str) {
  char buf[32], index;
  
  index = 0;
  while(val != 0) {
    buf[index] = (val % 10) + '0';
    val /= 10;
    index++;
  }
  buf[index] = '\n';
  index++;
  write(1, str, 3);
  write(1, buf, index);
  fflush(stdout);
}

/* Function declarations, so I can reorder them how I like */
void sh_create_arena(int index, int id, sicm_device *device, char invalid);

/*************************************************
 *               ORIG_MALLOC                     *
 *************************************************
 *  Used for allocating data structures in SICM
 *  after the malloc wrappers have been defined.
 */
void *__attribute__ ((noinline)) orig_malloc(size_t size) {
  return je_mallocx(size, MALLOCX_TCACHE_NONE);
  //return (*orig_malloc_ptr)(size);
}
void *__attribute__ ((noinline)) orig_calloc(size_t num, size_t size) {
  return je_mallocx(num * size, MALLOCX_TCACHE_NONE | MALLOCX_ZERO);
  //return (*orig_calloc_ptr)(num, size);
}
void *__attribute__ ((noinline)) orig_realloc(void *ptr, size_t size) {
  if(ptr == NULL) {
    return je_mallocx(size, MALLOCX_TCACHE_NONE);
  }
  return je_rallocx(ptr, size, MALLOCX_TCACHE_NONE);
  //return (*orig_realloc_ptr)(ptr, size);
}
void __attribute__ ((noinline)) orig_free(void *ptr) {
  je_dallocx(ptr, MALLOCX_TCACHE_NONE);
  //(*orig_free_ptr)(ptr);
}
void *__attribute__ ((noinline)) orig_valloc(size_t size) {
  return je_mallocx(size, MALLOCX_TCACHE_NONE | MALLOCX_ALIGN(4096));
  //return (*orig_valloc_ptr)(size);
}

/*************************************************
 *            PROFILE_ALLOCS                     *
 *************************************************
 *  Used to record each allocation. Enable with
 *  'enable_profile_allocs()'.
 */
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
    pthread_rwlock_unlock(&tracker.profile_allocs_map_lock);
    return;
  }

  /* Subtract from the size of the arena */
  tracker.arenas[aip->index]->size -= aip->size;

  /* Remove the allocation from the map */
  tree_delete(tracker.profile_allocs_map, ptr);
  pthread_rwlock_unlock(&tracker.profile_allocs_map_lock);
}

/*************************************************
 *            UTILITY FUNCTIONS                  *
 *************************************************
 *  Miscellaneous utility functions for allocation.
 */

/* Gets a unique index for a thread. Used especially
 * in per-thread arena layouts.
 */
int get_thread_index() {
  int *val;

  if(thread_index == -1) {
    /* Thread has not been assigned an index yet, so do that now */
    thread_index = tracker.current_thread_index++;
  }
  return thread_index;
}

/* Gets which arena index a site is allocated to. 
   Uses new_site to tell the caller if the arena is newly assigned. */
int get_site_arena(int id, char *new_site) {
  int ret;
  
  if(id > (tracker.max_sites - 1)) {
    fprintf(stderr, "Site %d goes over the maximum number of sites, %d. Aborting.\n", id, tracker.max_sites);
    exit(1);
  }

  if(new_site) {
    *new_site = 0;
  }
  
  ret = tracker.site_arenas[id];
  if(ret == -1) {
    /* This site doesn't have an arena yet, so grab the next available */
    ret = tracker.arena_counter++;
    tracker.site_arenas[id] = ret;
    
    if(new_site) {
      *new_site = 1;
    }
  }

  return ret;
}

sicm_device *get_arena_device(int index) {
  sicm_device_list devices;
  sicm_device *retval;
  
  /* We're not safe here, because the only place this is used checks to make
     sure the arena is there, and grabs a lock. */
  devices = sicm_arena_get_devices(tracker.arenas[index]->arena);
  if(devices.count > 0) {
    retval = devices.devices[0];
  }
  sicm_device_list_free(&devices);
  return retval;
}

/* Gets the device that this site has been assigned to. Returns
 * NULL if it's unset.
 */
sicm_device *get_site_device(int id) {
  deviceptr device;

  device = (deviceptr) tracker.site_devices[id];
  if(device == NULL) {
    device = tracker.default_device;
  }

  return device;
}

void set_site_device(int id, deviceptr device) {
  if(device == NULL) {
    device = tracker.default_device;
  }
  tracker.site_devices[id] = (atomic_int *) device;
}

/* Gets an offset (0 to `num_devices`) for the per-device arenas. */
int get_device_offset(deviceptr device) {
  int ret;
  
  /* This is a very fast method, but isn't very generalizable.
     Eventually that can be fixed, but I can't think of a safe
     way to associate a pointer with an index without locks (since
     this function runs every allocation). */
  if(device == tracker.upper_device) {
    ret = 0;
  } else if(device == tracker.lower_device) {
    ret = 1;
  } else {
    fprintf(stderr, "Device isn't upper_device or lower_device. Aborting.\n");
    exit(1);
  }
  
  return ret;
}

int get_big_small_arena(int id, size_t sz, deviceptr *device, char *new_site, char *invalid) {
  int ret;
  char prev_big;
  
  prev_big = tracker.site_bigs[id];
  
  if(new_site && (prev_big == -1)) {
    *new_site = 1;
  }
  
  if(prev_big == -1) {
    /* This is the condition that we haven't seen this site before, and that profiling is on.
       We set this variable so that the profiler knows to add it to the list of sites in the arena (which requires locking). */
    tracker.site_bigs[id] = 0;
  }
  
  if((sz + tracker.site_sizes[id] > tracker.big_small_threshold) && id) {
    /* If the site isn't already `big`, and if its size exceeds the threshold, mark it as `big`.
       Checked and set in this manner, the `site_bigs` atomics could be doubly set to `1`. That's fine. */
    tracker.site_bigs[id] = 1;
  }
  
  if(tracker.site_bigs[id] == 1) {
    ret = get_site_arena(id, NULL);
    ret += tracker.max_threads + 1; /* We need to skip the per-thread arenas. */
    *device = get_site_device(id);
    *invalid = 0;
  } else {
    ret = get_thread_index();
    *device = tracker.upper_device;
    *invalid = 1;
  }
  
  return ret;
}

/* Gets the index that the allocation site should go into */
int get_arena_index(int id, size_t sz) {
  int ret, thread_index;
  deviceptr device;
  siteinfo_ptr site;
  char new_site, invalid;

  invalid = 0;
  new_site = 0;
  ret = 0;
  device = NULL;
  switch(tracker.layout) {
    case ONE_ARENA:
      ret = 0;
      break;
    case EXCLUSIVE_ARENAS:
      /* One arena per thread. */
      thread_index = get_thread_index() + 1;
      ret = thread_index;
      break;
    case EXCLUSIVE_DEVICE_ARENAS:
      /* Two arenas per thread: one for each memory tier. */
      thread_index = get_thread_index();
      device = get_site_device(id);
      ret = get_device_offset(device);
      ret = (thread_index * tracker.arenas_per_thread) + ret + 1;
      break;
    case SHARED_SITE_ARENAS:
      /* One (shared) arena per allocation site. */
      ret = get_site_arena(id, &new_site);
      device = get_site_device(id);
      break;
    case BIG_SMALL_ARENAS:
      ret = get_big_small_arena(id, sz, &device, &new_site, &invalid);
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
  
  /* Assuming thread_index is specific to this thread,
     we don't need a lock here. */
  pending_index = ret;
  if(tracker.arenas[ret] == NULL) {
    /* We've *got* to grab a lock to create a new arena */
    if(pthread_mutex_lock(&tracker.arena_lock) != 0) {
      fprintf(stderr, "Failed to acquire arena lock. Aborting.\n");
      exit(1);
    }
    sh_create_arena(ret, id, device, invalid);
    if(pthread_mutex_unlock(&tracker.arena_lock) != 0) {
      fprintf(stderr, "Failed to unlock arena lock. Aborting.\n");
      exit(1);
    }
  } else if(new_site && should_profile()) {
    add_site_profile(ret, id);
  }
  (tracker.arenas[ret]->thread_allocs[get_thread_index()])++;
  
  return ret;
}

/*************************************************
 *             ARENA AND EXTENT ALLOC            *
 *************************************************
 *  Functions for creating arenas and extents.
 */
 
/* Adds an arena to the `arenas` array. */
void sh_create_arena(int index, int id, sicm_device *device, char invalid) {
  size_t i;
  arena_info *arena;
  siteinfo_ptr site;
  sicm_device_list dl;
  int err;
  
  /* Put an upper bound on the indices that need to be searched. */
  if(index > tracker.max_index) {
    tracker.max_index = index;
  }

  if(!device) {
    device = tracker.default_device;
  }

  arena = orig_calloc(1, sizeof(arena_info));
  arena->index = index;
  arena->thread_allocs = orig_calloc(tracker.max_threads, sizeof(int));
  dl.count = 1;
  dl.devices = orig_malloc(sizeof(sicm_device *) * 1);
  dl.devices[0] = device;
  arena->arena = sicm_arena_create(0, SICM_ALLOC_RELAXED, &dl);
  orig_free(dl.devices);

  /* Now add the arena to the array of arenas */
  tracker.arenas[index] = arena;
  
  /* Finally, tell the profiler about this arena */
  if(should_profile()) {
    create_arena_profile(index, id, invalid);
  }
}

/* Adds an extent to the `extents` array. */
void sh_create_extent(sarena *arena, void *start, void *end) {
  int arena_index;
  
  /* Get this thread's current arena index from `pending_indices` */
  arena_index = pending_index;

  /* A extent allocation is happening without an sh_alloc... */
  if(arena_index == -1) {
    fprintf(stderr, "Unknown extent allocation. Aborting.\n");
    exit(1);
  }

  if(pthread_rwlock_wrlock(&tracker.extents_lock) != 0) {
    fprintf(stderr, "Failed to acquire read/write lock. Aborting.\n");
    exit(1);
  }
  /* Finally, tell the profiler about this extent */
  extent_arr_insert(tracker.extents, start, end, tracker.arenas[arena_index]);
  if(should_profile_objmap()) {
    create_extent_objmap_entry(start, end);
  }
  if(pthread_rwlock_unlock(&tracker.extents_lock) != 0) {
    fprintf(stderr, "Failed to unlock read/write lock. Aborting.\n");
    exit(1);
  }
}

void sh_delete_extent(sarena *arena, void *start, void *end) {
  if(pthread_rwlock_wrlock(&tracker.extents_lock) != 0) {
    fprintf(stderr, "Failed to acquire read/write lock. Aborting.\n");
    exit(1);
  }
  extent_arr_delete(tracker.extents, start);
  //madvise(start, end - start, MADV_DONTNEED);
  /* Finally, tell the profiler that this extent is no more*/
  if(should_profile_objmap()) {
    delete_extent_objmap_entry(start);
  }
  if(pthread_rwlock_unlock(&tracker.extents_lock) != 0) {
    fprintf(stderr, "Failed to unlock read/write lock. Aborting.\n");
    exit(1);
  }
}

/*************************************************
 *              SH_ALLOC                         *
 *************************************************
 *  The primary interface. Allocation and deallocation
 *  functions to which the compiler pass creates calls.
 */

void* sh_realloc(int id, void *ptr, size_t sz) {
  int   index;
  void *ret;
  alloc_info_ptr aip;
  size_t new_usable_size, old_usable_size;
  char *charptr;
  
  old_usable_size = 0;
  if(!sh_initialized || (tracker.layout == INVALID_LAYOUT)) {
    if(ptr) {
      old_usable_size = je_malloc_usable_size(ptr);
    }
    ret = je_realloc(ptr, sz + sizeof(int));
    new_usable_size = je_malloc_usable_size(ret);
    charptr = ret;
    *((int *)(charptr + new_usable_size - sizeof(int))) = -1;
    return ret;
  } else if(id == 0) {
    if(ptr) {
      old_usable_size = je_malloc_usable_size(ptr);
    }
    ret = je_realloc(ptr, sz + sizeof(int));
    new_usable_size = je_malloc_usable_size(ret);
    charptr = ret;
    *((int *)(charptr + new_usable_size - sizeof(int))) = 0;
    tracker.site_sizes[0] -= old_usable_size;
    tracker.site_sizes[0] += new_usable_size;
    unaccounted -= old_usable_size;
    unaccounted += new_usable_size;
    //print_sizet(unaccounted, "re ");
    return ret;
  }
  
  index = get_arena_index(id, sz);
  if(ptr) {
    old_usable_size = je_malloc_usable_size(ptr);
  }
  ret = sicm_arena_realloc(tracker.arenas[index]->arena, ptr, sz + sizeof(int));
  new_usable_size = je_malloc_usable_size(ret);
  charptr = ret;
  *((int *)(charptr + new_usable_size - sizeof(int))) = id;
  tracker.site_sizes[id] -= old_usable_size;
  tracker.site_sizes[id] += new_usable_size;

  if(should_profile_allocs()) {
    profile_allocs_realloc(ptr, sz, index);
  }

  if (profopts.should_run_rdspy) {
    sh_rdspy_realloc(ptr, ret, sz, id);
  }

  return ret;
}

void* sh_alloc(int id, size_t sz) {
  int index;
  void *ret;
  alloc_info_ptr aip;
  size_t usable_size;
  char *charptr;

  if(!sz) {
    /* Here, some applications erroneously depend on the behavior that when
       size is 0, they'll get at least one byte back. Let's adhere to that
       assumption. */
    sz = 1;
  }
  if(!sh_initialized || (tracker.layout == INVALID_LAYOUT)) {
    ret = je_mallocx(sz + sizeof(int), MALLOCX_TCACHE_NONE);
    usable_size = je_malloc_usable_size(ret);
    charptr = ret;
    *((int *)(charptr + usable_size - sizeof(int))) = (int) -1;
    return ret;
  } else if(id == 0) {
    ret = je_mallocx(sz + sizeof(int), MALLOCX_TCACHE_NONE);
    usable_size = je_malloc_usable_size(ret);
    charptr = ret;
    *((int *)(charptr + usable_size - sizeof(int))) = (int) 0;
    tracker.site_sizes[0] += usable_size;
    unaccounted += usable_size;
    //print_sizet(unaccounted, "al ");
    return ret;
  }
  
  if(id >= tracker.max_sites) {
    fprintf(stderr, "Site %d went over maximum number of sites, %d.\n", id, tracker.max_sites);
    exit(1);
  }
  
  /* Here, we'll actually grab 4 more bytes than the application asked for */
  index = get_arena_index(id, sz);
  ret = sicm_arena_alloc(tracker.arenas[index]->arena, sz + sizeof(int));
  usable_size = je_malloc_usable_size(ret);
  charptr = ret;
  *((int *)(charptr + usable_size - sizeof(int))) = id;
  tracker.site_sizes[id] += usable_size;

  if(should_profile_allocs()) {
    profile_allocs_alloc(ret, sz, index);
  }
  
  if (profopts.should_run_rdspy) {
    sh_rdspy_alloc(ret, sz, id);
  }

  return ret;
}

void* sh_aligned_alloc(int id, size_t alignment, size_t sz) {
  int index;
  void *ret;
  size_t usable_size;
  char *charptr;

  if(!sz) {
    sz = alignment;
  }
  if(!sh_initialized || (tracker.layout == INVALID_LAYOUT)) {
    ret = je_mallocx(sz + sizeof(int), MALLOCX_TCACHE_NONE | MALLOCX_ALIGN(alignment));
    usable_size = je_malloc_usable_size(ret);
    charptr = ret;
    *((int *)(charptr + usable_size - sizeof(int))) = -1;
    return ret;
  } else if(id == 0) {
    ret = je_mallocx(sz + sizeof(int), MALLOCX_TCACHE_NONE | MALLOCX_ALIGN(alignment));
    usable_size = je_malloc_usable_size(ret);
    charptr = ret;
    *((int *)(charptr + usable_size - sizeof(int))) = 0;
    tracker.site_sizes[0] += usable_size;
    unaccounted += usable_size;
    //print_sizet(unaccounted, "aa ");
    return ret;
  }

  index = get_arena_index(id, sz);
  ret = sicm_arena_alloc_aligned(tracker.arenas[index]->arena, sz + sizeof(int), alignment);
  usable_size = je_malloc_usable_size(ret);
  charptr = ret;
  *((int *)(charptr + usable_size - sizeof(int))) = id;
  tracker.site_sizes[id] += usable_size;

  if(should_profile_allocs()) {
    profile_allocs_alloc(ret, sz, index);
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
  int id;
  size_t usable_size;
  char *charptr;
  
  if(!ptr) {
    return;
  }

  if(!sh_initialized || (tracker.layout == INVALID_LAYOUT)) {
    je_dallocx(ptr, MALLOCX_TCACHE_NONE);
    return;
  }

  if(profopts.should_run_rdspy) {
    sh_rdspy_free(ptr);
  }

  if(should_profile_allocs()) {
    profile_allocs_free(ptr);
  }

  /* Here, we know that at the end of this allocation, we've stored
     a 4-byte integer that represents the site ID. */
  usable_size = je_malloc_usable_size(ptr);
  charptr = ptr;
  id = *((int *)(charptr + usable_size - sizeof(int)));
  tracker.site_sizes[id] -= usable_size;
  if(id == 0) {
    unaccounted -= usable_size;
    //print_sizet(unaccounted, "fr ");
  }
  sicm_free(ptr);
}

void sh_sized_free(void* ptr, size_t size) {
  int id;
  size_t usable_size;
  char *charptr;
  
  if(!ptr) {
    return;
  }

  if(!sh_initialized || (tracker.layout == INVALID_LAYOUT)) {
    je_sdallocx(ptr, size, MALLOCX_TCACHE_NONE);
    return;
  }

  if(profopts.should_run_rdspy) {
    sh_rdspy_free(ptr);
  }

  if(should_profile_allocs()) {
    profile_allocs_free(ptr);
  }

  /* Here, we know that at the end of this allocation, we've stored
     a 4-byte integer that represents the site ID. */
  /* I don't know what to do with the `size` here, because
     SICM's low-level arena allocator doesn't support freeing a specific
     size. Most likely, this means that C++ sized allocations will break. */
  usable_size = je_malloc_usable_size(ptr);
  charptr = ptr;
  id = *((int *)(charptr + usable_size - sizeof(int)));
  tracker.site_sizes[id] -= usable_size;
  if(id == 0) {
    unaccounted -= usable_size;
    //print_sizet(unaccounted, "sf ");
  }
  sicm_free(ptr);
}
