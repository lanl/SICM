#pragma once
/* extent_arr is an array of jemalloc extents. Each element of
 * the array stores a start and end address, as well as a pointer to an arena.
 * This is designed to be extremely cache-friendly, and thus makes some 
 * sacrifices in the runtime complexity department. It's extremely fast
 * for use cases where we need to iterate over all of the extents in the array
 * very quickly, which we do when we rebind an arena or when we search
 * all allocated extents while profiling.
 */
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <pthread.h>

/* Stores information about a jemalloc extent */
typedef struct extent_info {
  void *start, *end;
  void *arena;
} extent_info;

typedef struct extent_arr {
  pthread_mutex_t mutex;
  size_t max_extents, index, deleted;
  extent_info *arr;
} extent_arr;

#define extent_arr_for(a, i) \
  for(i = 0; i < a->index; i++)

static inline extent_arr *extent_arr_init() {
  extent_arr *a;
  size_t i;

  a = (extent_arr *) malloc(sizeof(extent_arr));
  a->max_extents = 2;
  a->index = 0;
  a->deleted = 0;
  pthread_mutex_init(&a->mutex, NULL);
  a->arr = (extent_info *) malloc(sizeof(extent_info) * a->max_extents);
  extent_arr_for(a, i) {
    a->arr[i].start = NULL;
    a->arr[i].end = NULL;
    a->arr[i].arena = NULL;
  }
  return a;
}

static inline void extent_arr_insert(extent_arr *a, void *start, void *end, void *arena) {
  size_t old_max_extents, i, found;

  if(!a) {
    fprintf(stderr, "Extent array is NULL. Aborting.\n");
    exit(1);
  }

  pthread_mutex_lock(&a->mutex);

  /* First search for a blank spot */
  found = SIZE_MAX;
  if(a->deleted > 0) {
    extent_arr_for(a, i) {
      /* Empty element when both addresses are NULL */
      if(!a->arr[i].start && !a->arr[i].end) {
        found = i;
        break;
      }
    }
  }

  /* If we didn't find a deleted element, add to the array and possibly expand it */
  if(found == SIZE_MAX) {
    found = a->index;
    /* Now expand the array to allow for more items */
    if(a->index == a->max_extents - 1) {
      /* We need to reallocate */
      old_max_extents = a->max_extents;
      a->max_extents *= 2;
      a->arr = realloc(a->arr, a->max_extents * sizeof(extent_info));
      for(i = old_max_extents; i < a->max_extents; i++) {
        a->arr[i].start = NULL;
        a->arr[i].end = NULL;
        a->arr[i].arena = NULL;
      }
    }
    a->index++;
  } else {
    a->deleted--;
  }

  /* Insert either into the deleted element or what a->index used to be */
  a->arr[found].start = start;
  a->arr[found].end = end;
  a->arr[found].arena = arena;

  pthread_mutex_unlock(&a->mutex);
}

static inline void extent_arr_delete(extent_arr *a, void *start) {
  size_t i;

  if(!a) {
    fprintf(stderr, "Extent array is NULL. Aborting.\n");
    exit(1);
  }

  pthread_mutex_lock(&a->mutex);

  extent_arr_for(a, i) {
    if(a->arr[i].start == start) {
      a->arr[i].start = NULL;
      a->arr[i].end = NULL;
      a->arr[i].arena = NULL;
      a->deleted++;
      break;
    }
  }

  pthread_mutex_unlock(&a->mutex);
}

static inline void extent_arr_free(extent_arr *a) {
  free(a->arr);
  free(a);
}
