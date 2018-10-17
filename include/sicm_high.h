#pragma once

#include "sicm_low.h"
#include "sicm_impl.h"

enum arena_layout {
  SHARED_ONE_ARENA, /* One arena between all threads */
  EXCLUSIVE_ONE_ARENA, /* One arena per thread */
  SHARED_TWO_ARENAS, /* Two total arenas */
  EXCLUSIVE_TWO_ARENAS, /* Two arenas per thread */
  SHARED_SITE_ARENAS, /* One arena per allocation site */
  EXCLUSIVE_SITE_ARENAS, /* One arena per allocation site per thread */
  INVALID_LAYOUT
};

/* Keeps track of additional information about arenas for profiling */
typedef struct arena_info {
  unsigned index, id;
  sicm_arena arena;
  size_t accesses, rss, peak_rss;
} arena_info;

/* So we can access these things from profile.c.
 * These variables are defined in src/high/high.c.
 */
extern extent_arr *extents;
extern extent_arr *rss_extents;
extern arena_info **arenas;
extern int should_profile_all, should_profile_one, should_profile_rss;
extern char *profile_one_event, *profile_all_event;
extern int max_index;
extern int max_sample_pages;
extern int sample_freq;
extern int num_imcs, max_imc_len, max_event_len;
extern char **imcs;

#define DEFAULT_ARENA_LAYOUT INVALID_LAYOUT

__attribute__((constructor))
void sh_init();

__attribute__((destructor))
void sh_terminate();

void* sh_alloc_exact(int id, size_t sz);
void* sh_alloc(int id, size_t sz);

void sh_create_extent(void *begin, void *end);

void sh_free(void* ptr);
