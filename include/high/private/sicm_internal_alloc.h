#pragma once
#include <stdlib.h> /* For size_t */
#include <stdatomic.h>

#ifndef SICM_RUNTIME

/* This is for programs that just want to include `sicm_parsing.h` without
   bringing in and linking against the whole runtime library. All of these
   allocation functions should just call `libc`'s. */
static void *__attribute__ ((noinline)) internal_malloc(size_t size) {
  return malloc(size);
}

static void *__attribute__ ((noinline)) internal_calloc(size_t num, size_t size) {
  return calloc(num, size);
}

static void *__attribute__ ((noinline)) internal_realloc(void *ptr, size_t size) {
  return realloc(ptr, size);
}

static void __attribute__ ((noinline)) internal_free(void *ptr) {
  free(ptr);
  return;
}


#else
/* If we're in SICM's runtime library */

#include <jemalloc/jemalloc.h>
#include <pthread.h>
#include "sicm_extent_arr.h"
#include "proc_object_map.h"
typedef struct extent_arr extent_arr;
static extent_hooks_t internal_hooks;

/* Keep track of all internal extents */
extern pthread_rwlock_t internal_extents_lock;
extern extent_arr *internal_extents;
extern pthread_once_t internal_init_once;
extern unsigned internal_arena_ind;
extern struct proc_object_map_t internal_objmap;
extern pid_t internal_pid;
extern size_t peak_internal_usage_present_pages;
extern size_t peak_internal_usage_pages;
extern unsigned int internal_pagesize;
extern atomic_int internal_initialized;

/* This is an internal allocator that uses `jemalloc`. It places all
   SICM-internal allocations into an arena for easy bookkeeping. */
   
static void internal_init() {
  int err;
  size_t arena_ind_sz;
  extent_hooks_t *new_hooks;

  internal_initialized = 1;

  /* Initialize objmap to keep track of present pages in internal allocations, too */
  err = objmap_open(&internal_objmap);
  if (err < 0) {
    fprintf(stderr, "Failed to open the object map: %d. Aborting.\n", err);
    exit(1);
  }
  new_hooks = &internal_hooks;
  internal_extents = extent_arr_init();
  arena_ind_sz = sizeof(unsigned);
  internal_arena_ind = -1;
  internal_pid = getpid();
  internal_pagesize = sysconf(_SC_PAGE_SIZE);
  
  err = je_mallctl("arenas.create", (void *) &internal_arena_ind, &arena_ind_sz, (void *)&new_hooks, sizeof(extent_hooks_t *));
  if (err != 0) {
    fprintf(stderr, "Can't create the internal arena: %d\n", err);
    exit(1);
  }

  internal_initialized = 2;
}

/* Get the peak internal usage of PRESENT PAGES in SICM, in pages.
   If `output` is not NULL, it'll also print out extent address ranges. */
static size_t peak_internal_present_usage(FILE *output) {
  size_t i, tot;
  uint64_t start, end;
  int status;
  char entry_path_buff[4096];
  struct proc_object_map_record_t record;
  
  pthread_once(&internal_init_once, internal_init);
  
  pthread_rwlock_rdlock(&internal_extents_lock);
  
  tot = 0;
  extent_arr_for(internal_extents, i) {
    start = (uint64_t) internal_extents->arr[i].start;
    end = (uint64_t) internal_extents->arr[i].end;
    snprintf(entry_path_buff, sizeof(entry_path_buff),
             "/proc/%d/object_map/%lx-%lx", internal_pid, start, end);

    status = objmap_entry_read_record(entry_path_buff, &record);
    if (status == 0) {
      tot += record.n_resident_pages;
      if(output) {
        fprintf(output, "Internal: %p-%p\n", start, end);
      }
    }
  }
  if(tot > peak_internal_usage_present_pages) {
    peak_internal_usage_present_pages = tot;
  }
  
  if(output) {
    fflush(output);
  }
        
  pthread_rwlock_unlock(&internal_extents_lock);
  
  return peak_internal_usage_present_pages;
}

/* Get the peak internal usage of SICM, in pages */
static size_t peak_internal_usage() {
  size_t i, tot;
  uint64_t start, end;
  
  pthread_once(&internal_init_once, internal_init);
  
  pthread_rwlock_rdlock(&internal_extents_lock);
  
  tot = 0;
  extent_arr_for(internal_extents, i) {
    start = (uint64_t) internal_extents->arr[i].start;
    end = (uint64_t) internal_extents->arr[i].end;
    tot += (end - start) / internal_pagesize;
  }
  if(tot > peak_internal_usage_pages) {
    peak_internal_usage_pages = tot;
  }
  
  pthread_rwlock_unlock(&internal_extents_lock);
  
  return peak_internal_usage_pages;
}

static void *__attribute__ ((noinline)) internal_malloc(size_t size) {
  int flags;
  pthread_once(&internal_init_once, internal_init);
  if (size == 0) {
    size = 1;
  }
  flags = MALLOCX_ARENA(internal_arena_ind) | MALLOCX_TCACHE_NONE;
  return je_mallocx(size, flags);
}

static void *__attribute__ ((noinline)) internal_calloc(size_t num, size_t size) {
  int flags;
  void *ptr;
  pthread_once(&internal_init_once, internal_init);
  if (size == 0) {
    size = 1;
  }
  flags = MALLOCX_ARENA(internal_arena_ind) | MALLOCX_TCACHE_NONE | MALLOCX_ZERO;
  ptr = je_mallocx(num * size, flags);
  memset(ptr, 0, num * size);
  return ptr;
}

static void *__attribute__ ((noinline)) internal_realloc(void *ptr, size_t size) {
  int flags;
  
  pthread_once(&internal_init_once, internal_init);
  flags = MALLOCX_ARENA(internal_arena_ind) | MALLOCX_TCACHE_NONE;
  if(ptr) {
    return je_rallocx(ptr, size, flags);
  } else {
    return je_mallocx(size, flags);
  }
}

static void __attribute__ ((noinline)) internal_free(void *ptr) {
  pthread_once(&internal_init_once, internal_init);
  je_dallocx(ptr, MALLOCX_TCACHE_NONE);
}

static void *__attribute__ ((noinline)) internal_valloc(size_t size) {
  int flags;
  void *ptr;
  pthread_once(&internal_init_once, internal_init);
  if (size == 0) {
    size = 8;
  }
  flags = MALLOCX_ARENA(internal_arena_ind) | MALLOCX_TCACHE_NONE | MALLOCX_ALIGN(4096);
  ptr = je_mallocx(size, flags);
  if(!ptr) {
    fprintf(stderr, "internal_valloc failed. Aborting.\n");
    exit(1);
  }
  return ptr;
}

static void *internal_alloc(extent_hooks_t *, void *, size_t, size_t, bool *, bool *, unsigned);
static bool internal_dalloc(extent_hooks_t *, void *, size_t, bool, unsigned);
static void internal_destroy(extent_hooks_t *, void *, size_t, bool, unsigned);
static bool internal_commit(extent_hooks_t *, void *, size_t, size_t, size_t, unsigned);
static bool internal_decommit(extent_hooks_t *, void *, size_t, size_t, size_t, unsigned);
static bool internal_split(extent_hooks_t *, void *, size_t, size_t, size_t, bool, unsigned);
static bool internal_merge(extent_hooks_t *, void *, size_t, void *, size_t, bool, unsigned);

static extent_hooks_t internal_hooks = {
  .alloc = internal_alloc,
  .dalloc = internal_dalloc,
  .destroy = internal_destroy,
  .commit = internal_commit,
  .decommit = internal_decommit,
  .purge_lazy = NULL,
  .purge_forced = NULL,
  .split = internal_split,
  .merge = internal_merge,
};

static void *internal_alloc(extent_hooks_t *h, void *new_addr, size_t size, size_t alignment, bool *zero, bool *commit, unsigned arena_ind) {
  int mpol, err;
  unsigned flags;
  uintptr_t n, m;
  int oldmode, mmflags;
  void *ret;

  *commit = 0;
  *zero = 0;
  ret = NULL;

  pthread_rwlock_wrlock(&internal_extents_lock);
  if(internal_initialized != 2) {
    fprintf(stderr, "The internal allocator is still %d.\n", internal_initialized);
    fflush(stderr);
  }

  mmflags = MAP_ANONYMOUS|MAP_PRIVATE;
  ret = mmap(new_addr, size, PROT_READ | PROT_WRITE, mmflags, -1, 0);
  if (ret == MAP_FAILED) {
    ret = NULL;
    perror("mmap");
    goto failure;
  }

  if (alignment == 0 || ((uintptr_t) ret)%alignment == 0) {
    // we are lucky and got the right alignment
    goto success;
  }

  // the alignment didn't work out, munmap and try again
  munmap(ret, size);
  ret = NULL;

  // if new_addr is set, we can't fulfill the alignment, so just fail
  if (new_addr != NULL) {
    fprintf(stderr, "Can't fulfill the alignment of %zu because new_addr is set.\n", alignment);
    fflush(stderr);
    goto failure;
  }

  ret = mmap(NULL, size + alignment, PROT_READ | PROT_WRITE, mmflags, -1, 0);
  if (ret == MAP_FAILED) {
    perror("mmap2");
    ret = NULL;
    goto failure;
  }

  n = (uintptr_t) ret;
  m = n + alignment - (n % alignment);
  munmap(ret, m-n);
  munmap(ret + size, n % alignment);
  ret = (void *) m;

success:
  extent_arr_insert(internal_extents, ret, (char *)ret + size, NULL);
  err = objmap_add_range(&internal_objmap, ret, (char *)ret + size);
  if (err < 0) {
    fprintf(stderr, "WARNING: Couldn't add internal extent (start=%p, end=%p) to object_map (error = %d).\n", ret, (char *)ret + size, err);
  }
  
failure:
  pthread_rwlock_unlock(&internal_extents_lock);
  return ret;
}

static bool internal_dalloc(extent_hooks_t *h, void *addr, size_t size, bool committed, unsigned arena_ind) {
  bool ret, still_searching, partial, found_anything;
  int err;
  size_t i, leftover_size, num_extents;
  uint64_t start, end, partial_start, partial_end, target_ptr;

  ret = true;
#if 0
  pthread_rwlock_wrlock(&internal_extents_lock);

  /* We could have actually allocated more than jemalloc asked for, depending on alignment.
     Therefore we must search for this extent's actual size. */
  still_searching = true;
  target_ptr = addr;
  leftover_size = size;
  while(still_searching) {
    partial_start = 0;
    partial_end = 0;
    partial = false;
    start = 0;
    end = 0;
    found_anything = false;
    num_extents = 0;
    extent_arr_for(internal_extents, i) {
      start = (uint64_t) internal_extents->arr[i].start;
      end = (uint64_t) internal_extents->arr[i].end;
      num_extents++;
      if(start == target_ptr) {
        found_anything = true;
        if(leftover_size < (end - start)) {
          /* jemalloc is asking us to only unmap part of this extent */
          partial_start = start;
          partial_end = end;
          partial = true;
          still_searching = false;
          fprintf(stderr, "jemalloc wants to free %zu bytes, which is only part of our %p-%p extent.\n", leftover_size, partial_start, partial_end);
          fflush(stderr);
          target_ptr = NULL;
        } else if(leftover_size > (end - start)) {
          /* This extent is only part of the size that jemalloc wants to free.
             Our `end` pointer is the next pointer that we need to find. */
          partial = false;
          still_searching = true;
          fprintf(stderr, "jemalloc wants us to free %zu more bytes starting at %p. We'll free up %p-%p and more...\n",
                  leftover_size, target_ptr, start, end);
          fflush(stderr);
          target_ptr = end;
        } else {
          /* jemalloc wants us to free exactly this extent */
          partial = false;
          still_searching = false;
          target_ptr = NULL;
        }
        break;
      } else if((target_ptr > start) && (target_ptr < end)) {
      }
    }
    if(!found_anything) {
      fprintf(stderr, "Got a deallocation for %p of size %zu, but we didn't find any extents to match that out of %zu.\n",
              addr, size, num_extents);
      fflush(stderr);
    }
    if(partial) {
      /* Only free part of this extent. Put the remainder back in the extent array. */
      extent_arr_delete(internal_extents, partial_start);
      err = objmap_del_range(&internal_objmap, partial_start);
      if ((err < 0) && (err != -22)) {
        fprintf(stderr, "WARNING: Couldn't delete extent (%p) to object_map (error = %d).\n", partial_start, err);
        pthread_rwlock_unlock(&internal_extents_lock);
        exit(1);
      }
      extent_arr_insert(internal_extents, ((char *) partial_start) + leftover_size, partial_end, NULL);
      err = objmap_add_range(&internal_objmap, ((char *) partial_start) + leftover_size, partial_end);
      if (err < 0) {
        fprintf(stderr, "Couldn't add internal extent (start=%p, end=%p) while splitting to object_map (error = %d). Aborting.\n", ((char *) partial_start) + leftover_size, partial_end, err);
        pthread_rwlock_unlock(&internal_extents_lock);
        exit(1);
      }
      if (munmap(partial_start, leftover_size) != 0) {
        fprintf(stderr, "Failed to unmap %zu bytes starting at %p.\n", leftover_size, partial_start);
        pthread_rwlock_unlock(&internal_extents_lock);
        exit(1);
      }
      leftover_size = 0;
    } else {
      /* We'll free this whole extent (and possibly more).
         Here, leftover_size should always be larger than (end - start). */
      extent_arr_delete(internal_extents, start);
      err = objmap_del_range(&internal_objmap, start);
      if ((err < 0) && (err != -22)) {
        fprintf(stderr, "WARNING: Couldn't delete extent (%p) to object_map (error = %d).\n", start, err);
        pthread_rwlock_unlock(&internal_extents_lock);
        exit(1);
      }
      if (munmap(start, end - start) != 0) {
        fprintf(stderr, "Failed to unmap %zu bytes starting at %p.\n", end - start, start);
        pthread_rwlock_unlock(&internal_extents_lock);
        exit(1);
      }
      leftover_size -= (end - start);
    }
  }

  pthread_rwlock_unlock(&internal_extents_lock);
#endif
  return ret;
}

static void internal_destroy(extent_hooks_t *h, void *addr, size_t size, bool committed, unsigned arena_ind) {
  internal_dalloc(h, addr, size, committed, arena_ind);
}

static bool internal_commit(extent_hooks_t *h, void *addr, size_t size, size_t offset, size_t length, unsigned arena_ind) {
  return false;
}

static bool internal_decommit(extent_hooks_t *h, void *addr, size_t size, size_t offset, size_t length, unsigned arena_ind) {
  return true;
}

static bool internal_split(extent_hooks_t *h, void *addr, size_t size, size_t size_a, size_t size_b, bool committed, unsigned arena_ind) {
  return false;
}

static bool internal_merge(extent_hooks_t *h, void *addr_a, size_t size_a, void *addr_b, size_t size_b, bool committed, unsigned arena_ind) {
  return false;
}
#endif
