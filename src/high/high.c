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
#include <jemalloc/jemalloc.h>

#include "sicm_high.h"
#include "sicm_low.h"
#include "sicm_impl.h"
#include "sicm_profile.h"

/* !!!rdspy */
int get_thread_index();
#define MAX_TRIS 1024
ThreadReadsInfo * tris[MAX_TRIS];
int n_tris;

int should_run_rdspy;

void * recalloc(void * ptr, size_t new_bytes, size_t used_bytes) {
    void * new_region = calloc(new_bytes, 1);
    if (ptr) {
        memcpy(new_region, ptr, used_bytes);
        free(ptr);
    }
    return new_region;
}

SiteReadsAgg    agg_hist;
pthread_mutex_t sra_lock;
pthread_mutex_t tri_lock;
pthread_key_t   tri_key;

void SiteReadsAgg_init(SiteReadsAgg * sra) {
    sra->used_sites   = tree_make(unsigned, empty_tree_val);
    sra->site_map     = tree_make(addr_t, unsigned);
    sra->chunks_end   = 0;
    sra->histograms   = NULL;
    sra->n_histograms = 0;
}

void SiteReadsAgg_finish(SiteReadsAgg * sra) {
    FILE * f = fopen("read_times.csv", "w");

    fprintf(f, "site");
    for (int b = 0; b < READ_TIMES_MAX - READ_TIMES_BUCKET_SIZE; b += READ_TIMES_BUCKET_SIZE) {
        int next_b = b + READ_TIMES_BUCKET_SIZE;
        fprintf(f, ", %d - %d", b, next_b);
    }
    fprintf(f, ", %d+\n", READ_TIMES_MAX - READ_TIMES_BUCKET_SIZE);

    tree_it(unsigned, empty_tree_val) it;
    tree_traverse(sra->used_sites, it) {
        int sid = tree_it_key(it);

        fprintf(f, "%d", sid);
        for (int b = 0; b < READ_TIMES_NBUCKETS; b += 1) {
            fprintf(f, ", %llu", sra->histograms[sid][b]);
        }
        fprintf(f, "\n");
    }

    fclose(f);

    tree_free(sra->used_sites);
    tree_free(sra->site_map);
    if (sra->n_histograms)
        free(sra->histograms);
}

void SiteReadsAgg_give_histogram(SiteReadsAgg * sra, ThreadReadsInfo * tri) {
    tree_it(unsigned, empty_tree_val) it;
    tree_traverse(sra->used_sites, it) {
        unsigned site = tree_it_key(it);
        for (int b = 0; b < READ_TIMES_NBUCKETS; b += 1)
            sra->histograms[site][b] += tri->histograms[site][b];
    }
}

void ThreadReadsInfo_init(ThreadReadsInfo * tri) {
    tri->histograms   = calloc(agg_hist.n_histograms, sizeof(hist_t));
    tri->n_histograms = agg_hist.n_histograms;
}

void ThreadReadsInfo_finish(ThreadReadsInfo * tri) {
    for (int i = 0; i < MAX_TRIS; i += 1) {
        if (tris[i] == tri) {
            pthread_mutex_lock(&tri_lock);
            SiteReadsAgg_give_histogram(&agg_hist, tri);
            pthread_mutex_unlock(&tri_lock);

            if (tri->n_histograms)
                free(tri->histograms);
            free(tri);
            tris[i] = NULL;
            return;
        }
    }
}

static ThreadReadsInfo * get_tri() {
    ThreadReadsInfo * tri = pthread_getspecific(tri_key);
    if (tri == NULL) {
        pthread_mutex_lock(&tri_lock);
        if (pthread_getspecific(tri_key) == NULL) {
            int idx = get_thread_index();
            tri = (ThreadReadsInfo*)malloc(sizeof(ThreadReadsInfo));
            pthread_setspecific(tri_key, tri);
            ThreadReadsInfo_init(tri);
            tris[n_tris++] = tri;
        }
        pthread_mutex_unlock(&tri_lock);
    }

    return tri;
}
/* !!!end */

static struct sicm_device_list device_list;
static struct sicm_device *default_device;

tree(unsigned, deviceptr) site_nodes;
tree(deviceptr, int) device_arenas;

/* For profiling */
int should_profile_all; /* For sampling */
int should_profile_one; /* For bandwidth profiling */
int should_profile_rss;
struct sicm_device *profile_one_device;
char *profile_one_event;
char *profile_all_event;
int max_sample_pages;
int sample_freq;
int num_imcs, max_imc_len, max_event_len;
char **imcs; /* Array of strings of IMCs for the bandwidth profiling */

/* Keep track of all extents */
extent_arr *extents;
extent_arr *rss_extents; /* The extents that we want to get the RSS of */

/* Keeps track of arenas */
arena_info **arenas;
static enum arena_layout layout;
static int max_arenas, arenas_per_thread;
int max_index;

/* Associates a thread with an index (starting at 0) into the `arenas` array */
static pthread_key_t thread_key;
static int *thread_indices, *orig_thread_indices, *max_thread_indices, max_threads;

/* Passes an arena index to the extent hooks */
static int *pending_indices;

/* Takes a string as input and outputs which arena layout it is */
enum arena_layout parse_layout(char *env) {
	size_t max_chars;

	max_chars = 32;

  printf("Parsing layout: %s\n", env);

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
    case SHARED_DEVICE_ARENAS:
      return "SHARED_DEVICE_ARENAS";
    case EXCLUSIVE_DEVICE_ARENAS:
      return "EXCLUSIVE_DEVICE_ARENAS";
    case SHARED_SITE_ARENAS:
      return "SHARED_SITE_ARENAS";
    case EXCLUSIVE_SITE_ARENAS:
      return "EXCLUSIVE_SITE_ARENAS";
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
    if((device->tag == SICM_DRAM ||
       device->tag == SICM_KNL_HBM || 
       device->tag == SICM_POWERPC_HBM) &&
       sicm_numa_id(device) == id) {
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


/* Gets environment variables and sets up globals */
void set_options() {
  char *env, *endptr, *str, *line, guidance, found_guidance;
  long long tmp_val;
  struct sicm_device *device;
  int i, node;
  FILE *guidance_file;
  ssize_t len;
  unsigned site;
  tree_it(unsigned, deviceptr) it;

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

  /* Get max_arenas.
   * Keep in mind that 4096 is the maximum number supported by jemalloc.
   * An error occurs if this limit is reached.
   */
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

  /* Should we profile all allocation sites using sampling-based profiling? */
  env = getenv("SH_PROFILE_ALL");
  should_profile_all = 0;
  if(env) {
    should_profile_all = 1;
    printf("Profiling all arenas.\n");
  }

  /* Should we profile (by isolating) a single allocation site onto a NUMA node
   * and getting the memory bandwidth on that node?  Pass the allocation site
   * ID as the value of this environment variable.
   */
  env = getenv("SH_PROFILE_ONE");
  should_profile_one = 0;
  if(env) {
    endptr = NULL;
    tmp_val = strtoimax(env, &endptr, 10);
    if((tmp_val == 0) || (tmp_val > INT_MAX)) {
      printf("Invalid allocation site ID given: %d.\n", tmp_val);
      exit(1);
    } else {
      should_profile_one = (int) tmp_val;
    }
  }

  if(should_profile_one) {
    /* If the above is true, which NUMA node should we isolate the allocation site
     * onto? The user should also set SH_DEFAULT_DEVICE to another device to avoid
     * the two being the same, if the allocation site is to be isolated.
     */
    env = getenv("SH_PROFILE_ONE_NODE");
    if(env) {
      endptr = NULL;
      tmp_val = strtoimax(env, &endptr, 10);
      profile_one_device = get_device_from_numa_node((int) tmp_val);
      printf("Isolating node: %s, node %d\n", sicm_device_tag_str(profile_one_device->tag), 
                                              sicm_numa_id(profile_one_device));
    }

    /* The user can also specify a comma-delimited list of IMCs to read the
     * bandwidth from. This will be passed to libpfm. For example, on an Ivy
     * Bridge server, this value is e.g. `ivbep_unc_imc0`, and on KNL it's
     * `knl_unc_imc0`.
     */
    env = getenv("SH_PROFILE_ONE_IMC");
    num_imcs = 0;
    max_imc_len = 0;
    imcs = NULL;
    if(env) {
      printf("Got IMC string: %s\n", env);
      /* Parse out the IMCs into an array */
      while((str = strtok(env, ",")) != NULL) {
        printf("%s\n", str);
        num_imcs++;
        imcs = realloc(imcs, sizeof(char *) * num_imcs);
        imcs[num_imcs - 1] = str;
        if(strlen(str) > max_imc_len) {
          max_imc_len = strlen(str);
        }
        env = NULL;
      }
    }
    if(num_imcs == 0) {
      fprintf(stderr, "No IMCs given. Can't measure bandwidth.\n");
      exit(1);
    }

    /* What event should be used to measure the bandwidth? Default
     * to the hardcoded list in profile.c if not specified.
     */
    env = getenv("SH_PROFILE_ONE_EVENT");
    profile_one_event = NULL;
    max_event_len = 64;
    if(env) {
      profile_one_event = env;
      max_event_len = strlen(env);
      printf("Using event: %s\n", profile_one_event);
    }
  }

  /* Should we get the RSS of each arena? */
  env = getenv("SH_PROFILE_RSS");
  should_profile_rss = 0;
  if(env) {
    if(layout == SHARED_SITE_ARENAS) {
      should_profile_rss = 1;
      printf("Profiling RSS of all arenas.\n");
    } else {
      printf("Can't profile RSS, because we're using the wrong arena layout.\n");
    }
  }


  /* What sample frequency should we use? Default is 2048. Higher
   * frequencies will fill up the sample pages (below) faster.
   */
  env = getenv("SH_SAMPLE_FREQ");
  sample_freq = 2048;
  if(env) {
    endptr = NULL;
    tmp_val = strtoimax(env, &endptr, 10);
    if((tmp_val <= 0)) {
      printf("Invalid sample frequency given. Defaulting to %d.\n", sample_freq);
    } else {
      sample_freq = (int) tmp_val;
    }
  }
  printf("Sample frequency: %d\n", sample_freq);

  /* How many samples should be collected by perf, maximum?
   * Assuming we're only tracking addresses, this number is multiplied by 
   * the page size and divided by 16 to get the maximum number of samples.
   * 8 of those bytes are the header, and the other 8 are the address itself.
   * By default this is 64 pages, which yields 16k samples.
   */
  env = getenv("SH_MAX_SAMPLE_PAGES");
  max_sample_pages = 64;
  if(env) {
    endptr = NULL;
    tmp_val = strtoimax(env, &endptr, 10);
    /* Value needs to be non-negative, less than or equal to 512, and a power of 2. */
    if((tmp_val <= 0) || (tmp_val > 512) || (tmp_val & (tmp_val - 1))) {
      printf("Invalid number of pages given (%d). Defaulting to %d.\n", tmp_val, max_sample_pages);
    } else {
      max_sample_pages = (int) tmp_val;
    }
  }
  printf("Maximum sample pages: %d\n", max_sample_pages);

  /* Get default_device_tag */
  env = getenv("SH_DEFAULT_NODE");
  default_device = NULL;
  if(env) {
    endptr = NULL;
    tmp_val = strtoimax(env, &endptr, 10);
    default_device = get_device_from_numa_node((int) tmp_val);
  }
  if(!default_device) {
    default_device = device_list.devices;
  }
  printf("Default device: %s\n", sicm_device_tag_str(default_device->tag));

  /* Get arenas_per_thread */
  switch(layout) {
    case SHARED_ONE_ARENA:
    case EXCLUSIVE_ONE_ARENA:
      arenas_per_thread = 1;
      break;
    case SHARED_DEVICE_ARENAS:
    case EXCLUSIVE_DEVICE_ARENAS:
      arenas_per_thread = (int) device_list.count;
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

  /* Get the guidance file that tells where each site goes */
  env = getenv("SH_GUIDANCE_FILE");
  if(env) {
    /* Open the file */
    guidance_file = fopen(env, "r");
    if(!guidance_file) {
      fprintf(stderr, "Failed to open guidance file. Aborting.\n");
      exit(1);
    }

    /* Read in the sites */
    guidance = 0;
    found_guidance = 0; /* Set if we find any site guidance at all */
    line = NULL;
    len = 0;
    while(getline(&line, &len, guidance_file) != -1) {
      str = strtok(line, " ");
      if(guidance) {
        if(!str) continue;

        /* Look to see if it's the end */
        if(str && (strcmp(str, "=====") == 0)) {
          str = strtok(NULL, " ");
          if(str && (strcmp(str, "END") == 0)) {
            guidance = 0;
          } else {
            fprintf(stderr, "In a guidance section, and found five equals signs, but not the end. Aborting.\n");
            exit(1);
          }
          continue;
        }

        /* Read in the actual guidance now that we're in a guidance section */
        sscanf(str, "%u", &site);
        str = strtok(NULL, " ");
        if(!str) {
          fprintf(stderr, "Read in a site number from the guidance file, but no node number. Aborting.\n");
          exit(1);
        }
        sscanf(str, "%d", &node);
        tree_insert(site_nodes, site, get_device_from_numa_node(node));

      } else {
        if(!str) continue;
        /* Find the "===== GUIDANCE" tokens */
        if(strcmp(str, "=====") != 0) continue;
        str = strtok(NULL, " ");
        if(str && (strcmp(str, "GUIDANCE") == 0)) {
          /* Now we're in a guidance section */
          guidance = 1;
          found_guidance = 1;
          continue;
        }
      }
    }
    if(!found_guidance) {
      fprintf(stderr, "Didn't find any guidance in the file. Aborting.\n");
      exit(1);
    }
  }

  /* !!!rdspy */
  env = getenv("SH_RDSPY");
  should_run_rdspy = 0;
  if (env) {
      should_run_rdspy = 1;
      printf("Running with rdspy.\n");
  }
  /* !!!end */
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
  if(index > (max_arenas - 1)) {
    fprintf(stderr, "Maximum number of arenas reached. Aborting.\n");
    exit(1);
  }

  /* If we've already created this arena */
  if(arenas[index] != NULL) {
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
  arenas[index]->id = id;
  arenas[index]->rss = 0;
  arenas[index]->peak_rss = 0;
  arenas[index]->arena = sicm_arena_create(0, device);
}

/* Adds an extent to the `extents` array. */
void sh_create_extent(void *start, void *end) {
  int thread_index, arena_index;

  /* Get this thread's current arena index from `pending_indices` */
  thread_index = get_thread_index();
  arena_index = pending_indices[thread_index];

  /* A extent allocation is happening without an sh_alloc... */
  if(arena_index == 0) {
    fprintf(stderr, "Unknown extent allocation. Aborting.\n");
    exit(1);
  }

  if(should_profile_rss && (arenas[arena_index]->id == should_profile_one)) {
    /* If we're profiling RSS and this is the site that we're isolating */
    extent_arr_insert(rss_extents, start, end, arenas[arena_index]);
  }
  extent_arr_insert(extents, start, end, arenas[arena_index]);
}

/* Chooses an arena for the per-device arena layouts. */
int get_device_arena(int id, sicm_device **device) {
  tree_it(unsigned, deviceptr) it;
  tree_it(deviceptr, int) devit;
  int ret;

  /* One arena per device */
  it = tree_lookup(site_nodes, id);
  if(tree_it_good(it)) {
    /* This site was found in the guidance file.  Use its device pointer to
     * find if this device has already got an arena.
     */
    *device = tree_it_val(it);
  } else {
    /* Site's not in the guidance file. Use the default device. */
    *device = default_device;
  }
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

/* Gets the index that the ID should go into */
int get_arena_index(int id) {
  int ret, thread_index;
  sicm_device *device;

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
      ret = id;
      /* Special case for profiling */
      if(profile_one_device && (id == should_profile_one)) {
        /* If the site is the one we're profiling, isolate it */
        device = profile_one_device;
      }
      break;
    case EXCLUSIVE_SITE_ARENAS:
      ret = (thread_index * arenas_per_thread) + id;
      break;
    default:
      fprintf(stderr, "Invalid arena layout. Aborting.\n");
      exit(1);
      break;
  };


  pending_indices[thread_index] = ret;
  sh_create_arena(ret, id, device);

  return ret;
}

void* sh_realloc(int id, void *ptr, size_t sz) {
  int index;

  if(layout == INVALID_LAYOUT) {
    return realloc(ptr, sz);
  }

  index = get_arena_index(id);
  return sicm_arena_realloc(arenas[index]->arena, ptr, sz);
}

/* Accepts an allocation site ID and a size, does the allocation */
void* sh_alloc(int id, size_t sz) {
  int index;
  void *ret;

  if((layout == INVALID_LAYOUT) || !sz) {
    ret = je_malloc(sz);
  } else {
      if(id == should_profile_one) {
          printf("sh_alloc: %zu bytes to %d\n", sz, id);
      }
      index = get_arena_index(id);
      ret = sicm_arena_alloc(arenas[index]->arena, sz);
  }

  /* !!!rdspy */
  if (should_run_rdspy) {
      pthread_mutex_lock(&sra_lock);
      tree_insert(agg_hist.site_map, ret, id);
      tree_insert(agg_hist.used_sites, id, (empty_tree_val){});
      if (id + 1 > agg_hist.n_histograms) {
          agg_hist.histograms = recalloc(agg_hist.histograms, sizeof(hist_t) * (id + 1), sizeof(hist_t) * agg_hist.n_histograms);
          agg_hist.n_histograms = id + 1;
      }
      if (ret + sz > agg_hist.chunks_end)
          agg_hist.chunks_end = ret + sz;
      pthread_mutex_unlock(&sra_lock);

      for (int i = 0; i < n_tris; i += 1) {
          ThreadReadsInfo * tri = tris[i];
          if (id + 1 > tri->n_histograms) {
              tri->histograms = recalloc(tri->histograms, sizeof(hist_t) * (id + 1), sizeof(hist_t) * tri->n_histograms);
              tri->n_histograms = id + 1;
          }
      }
  }
  /* !!!end */
  
  return ret;
}

void sh_free(void* ptr) {
  if(layout == INVALID_LAYOUT) {
    je_free(ptr);
  } else {
    sicm_free(ptr);
  }
}

/* !!!rdspy */
unsigned long long sh_read(void * ptr, uint64_t beg, uint64_t end) {
    ThreadReadsInfo * tri;
    tree_it(addr_t, unsigned) it = tree_gtr(agg_hist.site_map, ptr);

    tree_it_prev(it);

    if (!tree_it_good(it)
    || ptr >= agg_hist.chunks_end)
        return 0;

    unsigned site  = tree_it_val(it);
    uint64_t ticks = end - beg;

    int bucket = ticks > READ_TIMES_MAX
                 ? READ_TIMES_NBUCKETS - 1
                 : ticks >> (READ_TIMES_BUCKET_SIZE / 2);

    tri = get_tri();

    tri->histograms[site][bucket] += 1;

    return 1;
}
/* !!!end */

__attribute__((constructor))
void sh_init() {
  int i;

  device_list = sicm_init();
  site_nodes = tree_make(unsigned, deviceptr);
  device_arenas = tree_make(deviceptr, int);
  set_options();
  
  if(layout != INVALID_LAYOUT) {
    /* `arenas` is a pseudo-two-dimensional array, first dimension is per-thread */
    /* Second dimension is one for each arena that each thread will have.
     * If the arena layout isn't per-thread (`EXCLUSIVE_`), arenas_per_thread is just
     * the total number of arenas.
     */
    switch(layout) {
      case SHARED_ONE_ARENA:
      case SHARED_DEVICE_ARENAS:
      case SHARED_SITE_ARENAS:
        arenas = (arena_info **) calloc(arenas_per_thread, sizeof(arena_info *));
        break;
      case EXCLUSIVE_SITE_ARENAS:
      case EXCLUSIVE_ONE_ARENA:
      case EXCLUSIVE_DEVICE_ARENAS:
        arenas = (arena_info **) calloc(max_threads * arenas_per_thread, sizeof(arena_info *));
        break;
    }

    /* Initialize the extents array.
     * If we're just doing MBI on one site, initialize a new array that has extents from just that site.
     * If we're profiling all sites, rss_extents is just all extents.
     */
    extents = extent_arr_init();
    if(should_profile_rss) {
      if(should_profile_one) {
        rss_extents = extent_arr_init();
      } else if(should_profile_all) {
        rss_extents = extents;
      }
    }

    /* Stores the index into the `arenas` array for each thread */
    pthread_key_create(&thread_key, NULL);
    thread_indices = (int *) malloc(max_threads * sizeof(int));
    orig_thread_indices = thread_indices;
    max_thread_indices = orig_thread_indices + max_threads;
    for(i = 0; i < max_threads; i++) {
      thread_indices[i] = i;
    }
    pthread_setspecific(thread_key, (void *) thread_indices);
    thread_indices++;

    /* Stores an index into `arenas` for the extent hooks */
    pending_indices = (int *) calloc(max_threads, sizeof(int));

    /* Set the arena allocator's callback function */
    sicm_extent_alloc_callback = &sh_create_extent;

    if(should_profile_all || should_profile_one || should_profile_rss) {
      sh_start_profile_thread();
    }
  }
  
  /* !!!rdspy */
    if (should_run_rdspy) {
      SiteReadsAgg_init(&agg_hist);
      pthread_mutex_init(&sra_lock, NULL);
      pthread_mutex_init(&tri_lock, NULL);
      pthread_key_create(&tri_key, (void(*)(void*))ThreadReadsInfo_finish);
  }
  /* !!!end */
}

__attribute__((destructor))
void sh_terminate() {
  size_t i;

  /* Clean up the low-level interface */
  sicm_fini(&device_list);

  if(layout != INVALID_LAYOUT) {

    /* Clean up the profiler */
    if(should_profile_all || should_profile_one || should_profile_rss) {
      sh_stop_profile_thread();
    }

    /* Clean up the arenas */
    for(i = 0; i <= max_index; i++) {
      if(!arenas[i]) continue;
      sicm_arena_destroy(arenas[i]->arena);
      free(arenas[i]);
    }
    free(arenas);

    free(pending_indices);
    free(orig_thread_indices);
    extent_arr_free(extents);
  }

  /* !!!rdspy */
  if (should_run_rdspy) {
      for (int i = 0; i < MAX_TRIS; i += 1) {
          if (tris[i]) {
              ThreadReadsInfo_finish(tris[i]);
          }
      }
      SiteReadsAgg_finish(&agg_hist);
  }
  /* !!!end */
}
