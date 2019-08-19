#include "sicm_high.h"

/* Options for profiling */
profiling_options profopts = {0};

/* Keeps track of arenas, extents, etc. */
tracker_struct tracker = {0};

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
  deviceptr retval;
  deviceptr device;
  int i;

  retval = NULL;
  /* Figure out which device the NUMA node corresponds to */
  device = *(tracker.device_list.devices);
  for(i = 0; i < tracker.device_list.count; i++) {
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
  if(retval == NULL) {
    fprintf(stderr, "Couldn't find an appropriate device for NUMA node %d.\n", id);
    exit(1);
  }

  return retval;
}

/* Gets environment variables and sets up globals */
void set_options() {
  char *env, *str, *line, guidance, found_guidance;
  long long tmp_val;
  deviceptr device;
  size_t i, n;
  int node, site;
  FILE *guidance_file;
  ssize_t len;
  tree_it(int, deviceptr) it;

  /* Do we want to use the online approach, moving arenas around devices automatically? */
  env = getenv("SH_ONLINE_PROFILING");
  profopts.should_profile_online = 0;
  if(env) {
    profopts.should_profile_online = 1;
    tmp_val = strtoimax(env, NULL, 10);
    profopts.online_device = get_device_from_numa_node((int) tmp_val);
    profopts.online_device_cap = sicm_avail(profopts.online_device) * 1024; /* sicm_avail() returns kilobytes */
  }

  /* Get the arena layout */
  env = getenv("SH_ARENA_LAYOUT");
  if(env) {
    tracker.layout = parse_layout(env);
  } else {
    tracker.layout = DEFAULT_ARENA_LAYOUT;
  }
  if(profopts.should_profile_online) {
    tracker.layout = SHARED_SITE_ARENAS;
  }

  /* Get max_threads */
  tracker.max_threads = numa_num_possible_cpus();
  env = getenv("SH_MAX_THREADS");
  if(env) {
    tmp_val = strtoimax(env, NULL, 10);
    if((tmp_val == 0) || (tmp_val > INT_MAX)) {
      fprintf(stderr, "Invalid thread number given. Aborting.\n");
      exit(1);
    } else {
      tracker.max_threads = (int) tmp_val;
    }
  }

  /* Get max_arenas.
   * Keep in mind that 4096 is the maximum number supported by jemalloc.
   * An error occurs if this limit is reached.
   */
  tracker.max_arenas = 4096;
  env = getenv("SH_MAX_ARENAS");
  if(env) {
    tmp_val = strtoimax(env, NULL, 10);
    if((tmp_val == 0) || (tmp_val > INT_MAX)) {
      fprintf(stderr, "Invalid arena number given. Aborting.\n");
      exit(1);
    } else {
      tracker.max_arenas = (int) tmp_val;
    }
  }

  /* Get max_sites_per_arena.
   * This is the maximum amount of allocation sites that a single arena can hold.
   */
  tracker.max_sites_per_arena = 1;
  env = getenv("SH_MAX_SITES_PER_ARENA");
  if(env) {
    tmp_val = strtoimax(env, NULL, 10);
    if((tmp_val == 0) || (tmp_val > INT_MAX)) {
      fprintf(stderr, "Invalid arena number given. Aborting.\n");
      exit(1);
    } else {
      tracker.max_sites_per_arena = (int) tmp_val;
    }
  }

  /* Controls the profiling rate of all profiling types */
  profopts.profile_rate_nseconds = 0;
  env = getenv("SH_PROFILE_RATE_NSECONDS");
  if(env) {
    profopts.profile_rate_nseconds = strtoimax(env, NULL, 10);
  }

  /* Should we profile all allocation sites using sampling-based profiling? */
  env = getenv("SH_PROFILE_ALL");
  profopts.should_profile_all = 0;
  if(env) {
    profopts.should_profile_all = 1;
  }
  if(profopts.should_profile_all) {

    env = getenv("SH_PROFILE_ALL_EVENTS");
    profopts.num_profile_all_events = 0;
    profopts.profile_all_events = NULL;
    if(env) {
      /* Parse out the events into an array */
      while((str = strtok(env, ",")) != NULL) {
        profopts.num_profile_all_events++;
        profopts.profile_all_events = realloc(profopts.profile_all_events, sizeof(char *) * profopts.num_profile_all_events);
        profopts.profile_all_events[profopts.num_profile_all_events - 1] = malloc(sizeof(char) * (strlen(str) + 1));
        strcpy(profopts.profile_all_events[profopts.num_profile_all_events - 1], str);
        env = NULL;
      }
    }
    if(profopts.num_profile_all_events == 0) {
      fprintf(stderr, "No profiling events given. Can't profile with sampling.\n");
      exit(1);
    }

    env = getenv("SH_PROFILE_ALL_SKIP_INTERVALS");
    profopts.profile_all_skip_intervals = 1;
    if(env) {
      profopts.profile_all_skip_intervals = strtoul(env, NULL, 0);
    }
  }

  /* Should we keep track of when each allocation happened, in intervals? */
  env = getenv("SH_PROFILE_ALLOCS");
  profopts.should_profile_allocs = 0;
  if(env) {
    profopts.should_profile_allocs = 1;
  }

  /* Should we profile (by isolating) a single allocation site onto a NUMA node
   * and getting the memory bandwidth on that node?
   */
  env = getenv("SH_PROFILE_ONE");
  profopts.should_profile_one = 0;
  if(env) {
    profopts.should_profile_one = 1;
  }

  if(profopts.should_profile_one) {

    if(profopts.should_profile_all) {
      fprintf(stderr, "Cannot enable both profile_all and profile_one. Choose one.\n");
      exit(1);
    }

    /* Which site are we profiling? */
    env = getenv("SH_PROFILE_ONE_SITE");
    profopts.profile_one_site = -1;
    if(env) {
      tmp_val = strtoimax(env, NULL, 10);
      if((tmp_val == 0) || (tmp_val > INT_MAX)) {
        fprintf(stderr, "Invalid allocation site ID given: %d.\n", tmp_val);
        exit(1);
      } else {
        profopts.profile_one_site = (int) tmp_val;
      }
    }

    /* If the above is true, which NUMA node should we isolate the allocation site
     * onto? The user should also set SH_DEFAULT_DEVICES to another device to avoid
     * the two being the same, if the allocation site is to be isolated.
     */
    env = getenv("SH_PROFILE_ONE_NODE");
    if(env) {
      tmp_val = strtoimax(env, NULL, 10);
      profopts.profile_one_device = get_device_from_numa_node((int) tmp_val);
    }

    /* The user can also specify a comma-delimited list of IMCs to read the
     * bandwidth from. This will be passed to libpfm. For example, on an Ivy
     * Bridge server, this value is e.g. `ivbep_unc_imc0`, and on KNL it's
     * `knl_unc_imc0`.
     */
    env = getenv("SH_PROFILE_ONE_IMC");
    profopts.num_imcs = 0;
    profopts.max_imc_len = 0;
    profopts.imcs = NULL;
    if(env) {
      /* Parse out the IMCs into an array */
      while((str = strtok(env, ",")) != NULL) {
        profopts.num_imcs++;
        profopts.imcs = realloc(profopts.imcs, sizeof(char *) * profopts.num_imcs);
        profopts.imcs[profopts.num_imcs - 1] = str;
        if(strlen(str) > profopts.max_imc_len) {
          profopts.max_imc_len = strlen(str);
        }
        env = NULL;
      }
    }
    if(profopts.num_imcs == 0) {
      fprintf(stderr, "No IMCs given. Can't profile one arena.\n");
      exit(1);
    }

    /* What events should be used to measure the bandwidth?
     */
    env = getenv("SH_PROFILE_ONE_EVENTS");
    profopts.num_profile_one_events = 0;
    char **tmp_profile_one_events = NULL;
    if(env) {
      /* Parse out the events into an array */
      while((str = strtok(env, ",")) != NULL) {
        profopts.num_profile_one_events++;
        tmp_profile_one_events = realloc(tmp_profile_one_events, sizeof(char *) * profopts.num_profile_one_events);
        tmp_profile_one_events[profopts.num_profile_one_events - 1] = str;
        env = NULL;
      }
    }
    if(profopts.num_profile_one_events == 0) {
      fprintf(stderr, "No profiling events given. Can't profile one arena.\n");
      exit(1);
    }

    /* Prepend each IMC name to each event string, because that's what libpfm4 expects */
    size_t index;
    profopts.profile_one_events = calloc(profopts.num_profile_one_events * profopts.num_imcs, sizeof(char *));
    for(i = 0; i < profopts.num_profile_one_events; i++) {
      for(n = 0; n < profopts.num_imcs; n++) {
        index = (i * profopts.num_imcs) + n;
        /* Allocate enough room for the IMC name, the event name, two colons, and a terminator. */
        profopts.profile_one_events[index] = malloc(sizeof(char) * 
                                    (strlen(tmp_profile_one_events[i]) + strlen(profopts.imcs[n]) + 3));
        sprintf(profopts.profile_one_events[index], "%s::%s", profopts.imcs[n], tmp_profile_one_events[i]);
      }
    }

    profopts.num_profile_one_events *= profopts.num_imcs;
  }

  /* Should we get the RSS of each arena? */
  env = getenv("SH_PROFILE_RSS");
  profopts.should_profile_rss = 0;
  if(env) {
    if(tracker.layout == SHARED_SITE_ARENAS) {
      profopts.should_profile_rss = 1;
    } else {
      fprintf(stderr, "Can't profile RSS, because we're using the wrong arena layout.\n");
      exit(1);
    }

    env = getenv("SH_PROFILE_RSS_SKIP_INTERVALS");
    profopts.profile_rss_skip_intervals = 1;
    if(env) {
      profopts.profile_rss_skip_intervals = strtoul(env, NULL, 0);
    }
  }

  env = getenv("SH_PROFILE_EXTENT_SIZE");
  profopts.should_profile_extent_size = 0;
  if(env) {
    profopts.should_profile_extent_size = 1;
    
    env = getenv("SH_PROFILE_EXTENT_SIZE_SKIP_INTERVALS");
    profopts.profile_extent_size_skip_intervals = 1;
    if(env) {
      profopts.profile_extent_size_skip_intervals = strtoul(env, NULL, 0);
    }
  }

  env = getenv("SH_PROFILE_ALLOCS");
  profopts.should_profile_allocs = 0;
  if(env) {
    profopts.should_profile_allocs = 1;
    
    env = getenv("SH_PROFILE_ALLOCS_SKIP_INTERVALS");
    profopts.profile_allocs_skip_intervals = 1;
    if(env) {
      profopts.profile_allocs_skip_intervals = strtoul(env, NULL, 0);
    }
  }

  /* What sample frequency should we use? Default is 2048. Higher
   * frequencies will fill up the sample pages (below) faster.
   */
  env = getenv("SH_SAMPLE_FREQ");
  profopts.sample_freq = 2048;
  if(env) {
    tmp_val = strtoimax(env, NULL, 10);
    if((tmp_val <= 0)) {
      fprintf(stderr, "Invalid sample frequency given. Aborting.\n");
      exit(1);
    } else {
      profopts.sample_freq = (int) tmp_val;
    }
  }

  /* How many samples should be collected by perf, maximum?
   * Assuming we're only tracking addresses, this number is multiplied by 
   * the page size and divided by 16 to get the maximum number of samples.
   * 8 of those bytes are the header, and the other 8 are the address itself.
   * By default this is 64 pages, which yields 16k samples.
   */
  env = getenv("SH_MAX_SAMPLE_PAGES");
  profopts.max_sample_pages = 64;
  if(env) {
    tmp_val = strtoimax(env, NULL, 10);
    /* Value needs to be non-negative, less than or equal to 512, and a power of 2. */
    if((tmp_val <= 0) || (tmp_val > 512) || (tmp_val & (tmp_val - 1))) {
      fprintf(stderr, "Invalid number of pages given. Aborting.\n");
      exit(1);
    } else {
      profopts.max_sample_pages = (int) tmp_val;
    }
  }

  /* Get default_device_tag */
  env = getenv("SH_DEFAULT_NODE");
  tracker.default_device = NULL;
  if(env) {
    tmp_val = strtoimax(env, NULL, 10);
    tracker.default_device = get_device_from_numa_node((int) tmp_val);
    printf("Defaulting to NUMA node %lld.\n", tmp_val);
  }
  if(!tracker.default_device) {
    /* This assumes that the normal page size is the first one that it'll find */
    if(env) {
      fprintf(stderr, "WARNING: Defaulting to NUMA node 0.\n");
    }
    tracker.default_device = get_device_from_numa_node(0);
  }

  /* Get arenas_per_thread */
  switch(tracker.layout) {
    case SHARED_ONE_ARENA:
    case EXCLUSIVE_ONE_ARENA:
      tracker.arenas_per_thread = 1;
      break;
    case SHARED_DEVICE_ARENAS:
    case EXCLUSIVE_DEVICE_ARENAS:
      tracker.arenas_per_thread = tracker.num_numa_nodes; //(int) device_list.count;
      break;
    case SHARED_SITE_ARENAS:
    case EXCLUSIVE_SITE_ARENAS:
      tracker.arenas_per_thread = tracker.max_arenas;
      break;
    case EXCLUSIVE_TWO_DEVICE_ARENAS:
      tracker.arenas_per_thread = 2 * tracker.num_numa_nodes; //((int) device_list.count);
      break;
    case EXCLUSIVE_FOUR_DEVICE_ARENAS:
      tracker.arenas_per_thread = 4 * tracker.num_numa_nodes; //((int) device_list.count);
      break;
    default:
      tracker.arenas_per_thread = 1;
      break;
  };

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
        sscanf(str, "%d", &site);
        str = strtok(NULL, " ");
        if(!str) {
          fprintf(stderr, "Read in a site number from the guidance file, but no node number. Aborting.\n");
          exit(1);
        }
        sscanf(str, "%d", &node);
        tree_insert(tracker.site_nodes, site, get_device_from_numa_node(node));
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

  env = getenv("SH_NUM_STATIC_SITES");
  if (env) {
    tmp_val = strtoimax(env, NULL, 10);
    if((tmp_val == 0) || (tmp_val > INT_MAX)) {
      fprintf(stderr, "Invalid number of static sites given. Aborting.\n");
      exit(1);
    } else {
      tracker.num_static_sites = (int) tmp_val;
    }
  }

  env = getenv("SH_RDSPY");
  profopts.should_run_rdspy = 0;
  if (env) {
    if (!tracker.num_static_sites) {
      fprintf(stderr, "Invalid number of static sites. Aborting.\n");
      exit(1);
    }
    profopts.should_run_rdspy = 1 && tracker.num_static_sites;
  }
}

__attribute__((constructor))
void sh_init() {
  int i;
  long size;

  tracker.device_list = sicm_init();
  pthread_rwlock_init(&tracker.extents_lock, NULL);
  pthread_mutex_init(&tracker.arena_lock, NULL);
  pthread_mutex_init(&tracker.thread_lock, NULL);

  /* Get the number of NUMA nodes with memory, since we ignore huge pages with
   * the DEVICE arena layouts */
  tracker.num_numa_nodes = 0;
  for(i = 0; i <= numa_max_node(); i++) {
    size = -1;
    if ((numa_node_size(i, &size) != -1) && size) {
      tracker.num_numa_nodes++;
    }
  }

  tracker.arena_counter = 0;
  tracker.site_arenas = tree_make(int, int);
  tracker.site_nodes = tree_make(int, deviceptr);
  tracker.device_arenas = tree_make(deviceptr, int);
  set_options();
  
  if(tracker.layout != INVALID_LAYOUT) {
    /* `arenas` is a pseudo-two-dimensional array, first dimension is per-thread.
     * Second dimension is one for each arena that each thread will have.
     * If the arena layout isn't per-thread (`EXCLUSIVE_`), arenas_per_thread is just
     * the total number of arenas.
     */
    tracker.arenas = (arena_info **) calloc(tracker.max_arenas, sizeof(arena_info *));

    /* Initialize the extents array.
     */
    tracker.extents = extent_arr_init();

    /* Stores the index into the `arenas` array for each thread */
    pthread_key_create(&tracker.thread_key, NULL);
    tracker.thread_indices = (int *) malloc(tracker.max_threads * sizeof(int));
    tracker.orig_thread_indices = tracker.thread_indices;
    tracker.max_thread_indices = tracker.orig_thread_indices + tracker.max_threads;
    for(i = 0; i < tracker.max_threads; i++) {
      tracker.thread_indices[i] = i;
    }
    pthread_setspecific(tracker.thread_key, (void *) tracker.thread_indices);
    tracker.thread_indices++;

    /* Stores an index into `arenas` for the extent hooks */
    tracker.pending_indices = (int *) malloc(tracker.max_threads * sizeof(int));
    for(i = 0; i < tracker.max_threads; i++) {
      tracker.pending_indices[i] = -1;
    }

    /* Set the arena allocator's callback function */
    sicm_extent_alloc_callback = &sh_create_extent;
    sicm_extent_dalloc_callback = &sh_delete_extent;

    profopts.should_profile = 0;
    if(profopts.should_profile_all ||
       profopts.should_profile_rss ||
       profopts.should_profile_extent_size ||
       profopts.should_profile_online ||
       profopts.should_profile_allocs ||
       profopts.should_profile_one) {
      profopts.should_profile = 1;
    }

    if(profopts.should_profile) {
      sh_start_profile_master_thread();
    }
  }
  
  if (profopts.should_run_rdspy) {
    sh_rdspy_init(tracker.max_threads, tracker.num_static_sites);
  }
}

__attribute__((destructor))
void sh_terminate() {
  size_t i;

  /* Clean up the low-level interface */
  sicm_fini(&tracker.device_list);

  if(tracker.layout != INVALID_LAYOUT) {

    /* Clean up the profiler */
    if(profopts.should_profile) {
      sh_stop_profile_master_thread();
    }

    /* Clean up the arenas */
    for(i = 0; i <= tracker.max_index; i++) {
      if(!tracker.arenas[i]) continue;
      sicm_arena_destroy(tracker.arenas[i]->arena);
      free(tracker.arenas[i]);
    }
    free(tracker.arenas);

    free(tracker.pending_indices);
    free(tracker.orig_thread_indices);
    extent_arr_free(tracker.extents);
  }

  if (profopts.should_run_rdspy) {
      sh_rdspy_terminate();
  }
}
