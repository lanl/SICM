#include <fcntl.h>
#include <numa.h>
#include <stdlib.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <stdio.h>
#include <errno.h>
#include <string.h>
#include <inttypes.h>
#include <pthread.h>

#include "high.h"
#include "sicm_low.h"
#include "sicmimpl.h"
#include "profile.h"

struct sicm_device_list device_list;
enum sicm_device_tag default_device_tag;
struct sicm_device *default_device;
enum arena_layout layout;
int max_threads, max_arenas, max_index;
int should_profile;
int arenas_per_thread;
arena_info **arenas;
pthread_key_t thread_key;
int *thread_indices, *orig_thread_indices;

/* Takes a string as input and outputs which arena layout it is */
enum arena_layout parse_layout(char *env) {
	size_t max_chars;

	max_chars = 32;

	if(strncmp(env, "SHARED_ONE_ARENA", max_chars) == 0) {
		return SHARED_ONE_ARENA;
	} else if(strncmp(env, "EXCLUSIVE_ONE_ARENA", max_chars) == 0) {
		return EXCLUSIVE_ONE_ARENA;
	} else if(strncmp(env, "SHARED_TWO_ARENAS", max_chars) == 0) {
		return SHARED_TWO_ARENAS;
	} else if(strncmp(env, "EXCLUSIVE_TWO_ARENAS", max_chars) == 0) {
		return EXCLUSIVE_TWO_ARENAS;
	} else if(strncmp(env, "SHARED_SITE_ARENAS", max_chars) == 0) {
		return SHARED_SITE_ARENAS;
	} else if(strncmp(env, "EXCLUSIVE_SITE_ARENAS", max_chars) == 0) {
		return EXCLUSIVE_SITE_ARENAS;
	}

  return INVALID_LAYOUT;
}

/* Converts an arena_layout to a string */
char *layout_str(enum arena_layout layout) {
  switch(layout) {
    case SHARED_ONE_ARENA:
      return "SHARED_ONE_ARENA";
    case EXCLUSIVE_ONE_ARENA:
      return "EXCLUSIVE_ONE_ARENA";
    case SHARED_TWO_ARENAS:
      return "SHARED_TWO_ARENAS";
    case EXCLUSIVE_TWO_ARENAS:
      return "EXCLUSIVE_TWO_ARENAS";
    case SHARED_SITE_ARENAS:
      return "SHARED_SITE_ARENAS";
    case EXCLUSIVE_SITE_ARENAS:
      return "EXCLUSIVE_SITE_ARENAS";
    default:
      break;
  }

  return "INVALID_LAYOUT";
}

/* Gets environment variables and sets up globals */
void set_options() {
  char *env, *endptr;
  long long tmp_val;
  struct sicm_device *device;
  int i;

  /* Get the arena layout */
  env = getenv("SH_ARENA_LAYOUT");
  if(env) {
    layout = parse_layout(env);
  } else {
    layout = DEFAULT_ARENA_LAYOUT;
  }
  printf("Arena layout: %s\n", layout_str(layout));

  /* Get max_threads */
  max_threads = numa_num_possible_cpus();
  env = getenv("SH_MAX_THREADS");
  if(env) {
    endptr = NULL;
    tmp_val = strtoimax(env, &endptr, 10);
    if((tmp_val == 0) || (tmp_val > INT_MAX)) {
      printf("Invalid thread number given. Defaulting to %d.\n", max_threads);
    } else {
      max_threads = (int) tmp_val;
    }
  }
  printf("Maximum threads: %d\n", max_threads);

  /* Get max_arenas */
  max_arenas = 4096;
  env = getenv("SH_MAX_ARENAS");
  if(env) {
    endptr = NULL;
    tmp_val = strtoimax(env, &endptr, 10);
    if((tmp_val == 0) || (tmp_val > INT_MAX)) {
      printf("Invalid arena number given. Defaulting to %d.\n", max_arenas);
    } else {
      max_arenas = (int) tmp_val;
    }
  }
  printf("Maximum arenas: %d\n", max_arenas);

  /* Should we profile? */
  env = getenv("SH_PROFILING");
  should_profile = 0;
  if(env) {
    should_profile = 1;
    printf("Profiling.\n");
  }

  /* Get default_device_tag */
  env = getenv("SH_DEFAULT_DEVICE");
  if(env) {
    default_device_tag = sicm_get_device_tag(env);
  } else {
    default_device_tag = INVALID_TAG;
  }
  printf("Default device tag: %s.\n", sicm_device_tag_str(default_device_tag));
  
  /* Get default_device */
  if(default_device_tag == INVALID_TAG) {
    default_device = device_list.devices;
  } else {
    device = device_list.devices;
    for(i = 0; i < device_list.count; i++) {
      if(device->tag == default_device_tag) {
        break;
      }
      device++;
    }
    if(device == device_list.devices + device_list.count) {
      device = device_list.devices;
    }
    default_device = device;
  }
  printf("Default device: %s\n", sicm_device_tag_str(default_device->tag));

  /* Get arenas_per_thread */
  switch(layout) {
    case SHARED_ONE_ARENA:
    case EXCLUSIVE_ONE_ARENA:
      arenas_per_thread = 1;
      break;
    case SHARED_TWO_ARENAS:
    case EXCLUSIVE_TWO_ARENAS:
      arenas_per_thread = 2;
      break;
    case SHARED_SITE_ARENAS:
    case EXCLUSIVE_SITE_ARENAS:
      arenas_per_thread = max_arenas;
      break;
    default:
      arenas_per_thread = 1;
      break;
  };
  printf("Arenas per thread: %d\n", arenas_per_thread);
}

int get_thread_index() {
  int *val;

  /* Get this thread's index */
  val = (int *) pthread_getspecific(thread_key);

  /* If nonexistent, increment the counter and set it */
  if(val == NULL) {
    pthread_setspecific(thread_key, (void *) thread_indices);
    val = thread_indices;
    thread_indices++;
  }

  return *val;
}

/* Gets the index that the ID should go into */
int get_arena_index(int id) {
  int ret, tmp;

  /* First get the index that we'll return */
  ret = 0;
  switch(layout) {
    case SHARED_ONE_ARENA:
      ret = 0;
      break;
    case EXCLUSIVE_ONE_ARENA:
      ret = get_thread_index();
      break;
    case SHARED_TWO_ARENAS:
      /* Should have one hot and one cold arena */
      break;
    case EXCLUSIVE_TWO_ARENAS:
      /* Same */
      break;
    case SHARED_SITE_ARENAS:
      ret = id;
      break;
    case EXCLUSIVE_SITE_ARENAS:
      tmp = get_thread_index();
      ret = (tmp * arenas_per_thread) + id;
      break;
    default:
      fprintf(stderr, "Invalid arena layout. Aborting.\n");
      exit(1);
      break;
  };

  /* Put an upper bound on the indices that need to be searched */
  if(ret > max_index) {
    max_index = ret;
  }

  /* Create the arena if it doesn't exist */
  if(arenas[ret] == NULL) {
    arenas[ret] = calloc(1, sizeof(arena_info));
    arenas[ret]->index = ret;
    arenas[ret]->accesses = 0;
    arenas[ret]->id = id;
    arenas[ret]->arena = sicm_arena_create(0, default_device);
  }

  return ret;
}

void* sh_realloc(int id, void *ptr, size_t sz) {
  if(layout == INVALID_LAYOUT) {
    return realloc(ptr, sz);
  }

  return sicm_realloc(ptr, sz);
}

/* Accepts an allocation site ID and a size, does the allocation */
void* sh_alloc(int id, size_t sz) {
  int index;
  void *ptr;

  if(layout == INVALID_LAYOUT) {
    return je_malloc(sz);
  }

  index = get_arena_index(id);
  return sicm_arena_alloc(arenas[index]->arena, sz);
}

void sh_free(void* ptr) {
  if(layout == INVALID_LAYOUT) {
    je_free(ptr);
  } else {
    sicm_free(ptr);
  }
}

__attribute__((constructor))
void sh_init() {
  int i;

  device_list = sicm_init();
  set_options();
  
  if(layout != INVALID_LAYOUT) {
    /* `arenas` is a pseudo-two-dimensional array, first dimension is per-thread */
    /* Second dimension is one for each arena that each thread will have */
    arenas = (arena_info **) calloc(max_threads * arenas_per_thread, sizeof(arena_info *));

    /* Stores the index into the `arenas` array for each thread */
    pthread_key_create(&thread_key, NULL);
    thread_indices = (int *) malloc(max_threads * sizeof(int));
    orig_thread_indices = thread_indices;
    for(i = 0; i < max_threads; i++) {
      thread_indices[i] = i;
    }
    pthread_setspecific(thread_key, (void *) thread_indices);
    thread_indices++;

    if(should_profile) {
      sh_start_profile_thread();
    }
  }
}

__attribute__((destructor))
void sh_terminate() {
  if(layout != INVALID_LAYOUT) {
    if(should_profile) {
      sh_stop_profile_thread();
    }
    free(arenas);
    free(orig_thread_indices);
  }
  MEM5_CREATE_MEMMAP;
}
