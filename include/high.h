#pragma once

#include "sicm_low.h"

typedef struct arena_info {
  unsigned index;
  sicm_arena *arena;
} arena_info;

enum arena_layout {
  SHARED_ONE_ARENA, /* One arena between all threads */
  EXCLUSIVE_ONE_ARENA, /* One arena per thread */
  SHARED_TWO_ARENAS, /* Two total arenas */
  EXCLUSIVE_TWO_ARENAS, /* Two arenas per thread */
  SHARED_SITE_ARENAS, /* One arena per allocation site */
  EXCLUSIVE_SITE_ARENAS, /* One arena per allocation site per thread */
  INVALID_LAYOUT
};

#define DEFAULT_ARENA_LAYOUT SHARED_SITE_ARENAS

__attribute__((constructor))
void sh_init();

__attribute__((destructor))
void sh_terminate();

void* sh_alloc_exact(int id, size_t sz);

void sh_free(void* ptr);
