#pragma once

#include "sicm_low.h"
#include "sicmimpl.h"

enum arena_layout {
  SHARED_ONE_ARENA, /* One arena between all threads */
  EXCLUSIVE_ONE_ARENA, /* One arena per thread */
  SHARED_TWO_ARENAS, /* Two total arenas */
  EXCLUSIVE_TWO_ARENAS, /* Two arenas per thread */
  SHARED_SITE_ARENAS, /* One arena per allocation site */
  EXCLUSIVE_SITE_ARENAS, /* One arena per allocation site per thread */
  INVALID_LAYOUT
};

extern int max_threads, max_arenas, max_index;
extern int arenas_per_thread;
extern arena_info **arenas;

#define DEFAULT_ARENA_LAYOUT SHARED_SITE_ARENAS

__attribute__((constructor))
void sh_init();

__attribute__((destructor))
void sh_terminate();

void* sh_alloc_exact(int id, size_t sz);

void sh_free(void* ptr);
