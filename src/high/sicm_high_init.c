#include "sicm_high.h"
#include "sicm_low.h"
#include "sicm_impl.h"
#include "sicm_profile.h"

profiling_options profopts = {0};

/* Gets environment variables and sets up globals */
void set_options() {
  char *env, *str, *line, guidance, found_guidance;
  long long tmp_val;
  struct sicm_device *device;
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
    online_device = get_device_from_numa_node((int) tmp_val);
    profopts.online_device_cap = sicm_avail(online_device) * 1024; /* sicm_avail() returns kilobytes */
    printf("Doing online profiling, packing onto NUMA node %lld with a capacity of %zd.\n", tmp_val, profopts.online_device_cap);
  }

  /* Get the arena layout */
  env = getenv("SH_ARENA_LAYOUT");
  if(env) {
    layout = parse_layout(env);
  } else {
    layout = DEFAULT_ARENA_LAYOUT;
  }
  if(profopts.should_profile_online) {
    layout = SHARED_SITE_ARENAS;
  }
  printf("Arena layout: %s\n", layout_str(layout));

  /* Get max_threads */
  max_threads = numa_num_possible_cpus();
  env = getenv("SH_MAX_THREADS");
  if(env) {
    tmp_val = strtoimax(env, NULL, 10);
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
    tmp_val = strtoimax(env, NULL, 10);
    if((tmp_val == 0) || (tmp_val > INT_MAX)) {
      printf("Invalid arena number given. Defaulting to %d.\n", max_arenas);
    } else {
      max_arenas = (int) tmp_val;
    }
  }
  printf("Maximum arenas: %d\n", max_arenas);

  /* Get max_sites_per_arena.
   * This is the maximum amount of allocation sites that a single arena can hold.
   */
  max_sites_per_arena = 1;
  env = getenv("SH_MAX_SITES_PER_ARENA");
  if(env) {
    tmp_val = strtoimax(env, NULL, 10);
    if((tmp_val == 0) || (tmp_val > INT_MAX)) {
      printf("Invalid arena number given. Defaulting to %d.\n", max_arenas);
    } else {
      max_sites_per_arena = (int) tmp_val;
    }
  }
  printf("Maximum allocation sites per arena: %d\n", max_sites_per_arena);

  /* Should we profile all allocation sites using sampling-based profiling? */
  env = getenv("SH_PROFILE_ALL");
  profopts.should_profile_all = 0;
  if(env) {
    profopts.should_profile_all = 1;
  }
  /* The online approach requires online profiling */
  if(profopts.should_profile_online) {
    profopts.should_profile_all = 1;
  }
  if(profopts.should_profile_all) {
    profopts.profile_all_rate = 1.0;
    if(profopts.should_profile_all) {
      env = getenv("SH_PROFILE_ALL_RATE");
      if(env) {
        profopts.profile_all_rate = strtof(env, NULL);
      }
    }

    env = getenv("SH_PROFILE_ALL_EVENTS");
    profopts.num_profile_all_events = 0;
    profopts.profile_all_events = NULL;
    if(env) {
      /* Parse out the events into an array */
      while((str = strtok(env, ",")) != NULL) {
        profopts.num_profile_all_events++;
        profopts.profile_all_events = realloc(profopts.profile_all_events, sizeof(char *) * profopts.num_profile_all_events);
        profopts.profile_all_events[profopts.num_profile_all_events - 1] = str;
        env = NULL;
      }
    }
    if(profopts.num_profile_all_events == 0) {
      fprintf(stderr, "No profiling events given. Can't profile with sampling.\n");
      exit(1);
    }

    profopts.num_events = profopts.num_profile_all_events;
    profopts.events = profopts.profile_all_events;
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
     * onto? The user should also set SH_DEFAULT_DEVICE to another device to avoid
     * the two being the same, if the allocation site is to be isolated.
     */
    env = getenv("SH_PROFILE_ONE_NODE");
    if(env) {
      tmp_val = strtoimax(env, NULL, 10);
      profopts.profile_one_device = get_device_from_numa_node((int) tmp_val);
      printf("Isolating node: %s, node %d\n", sicm_device_tag_str(profopts.profile_one_device->tag), 
                                              sicm_numa_id(profopts.profile_one_device));
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
      printf("Got IMC string: %s\n", env);
      /* Parse out the IMCs into an array */
      while((str = strtok(env, ",")) != NULL) {
        printf("%s\n", str);
        profopts.num_imcs++;
        profopts.imcs = realloc(imcs, sizeof(char *) * profopts.num_imcs);
        imcs[profopts.num_imcs - 1] = str;
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
        printf("Profiling one arena with event: %s\n", str);
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
    profopts.num_events = profopts.num_profile_one_events;
    profopts.events = profopts.profile_one_events;
  }

  /* Should we get the RSS of each arena? */
  env = getenv("SH_PROFILE_RSS");
  profopts.should_profile_rss = 0;
  if(env) {
    if(layout == SHARED_SITE_ARENAS) {
      profopts.should_profile_rss = 1;
      printf("Profiling RSS of all arenas.\n");
    } else {
      printf("Can't profile RSS, because we're using the wrong arena layout.\n");
    }
  }
  if(profopts.should_profile_online) {
    profopts.should_profile_rss = 1;
  }
  profopts.profile_rss_rate = 1.0;
  if(profopts.should_profile_rss) {
    env = getenv("SH_PROFILE_RSS_RATE");
    if(env) {
      profopts.profile_rss_rate = strtof(env, NULL);
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
      printf("Invalid sample frequency given. Defaulting to %d.\n", profopts.sample_freq);
    } else {
      profopts.sample_freq = (int) tmp_val;
    }
  }
  printf("Sample frequency: %d\n", profopts.sample_freq);

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
      printf("Invalid number of pages given (%d). Defaulting to %d.\n", tmp_val, profopts.max_sample_pages);
    } else {
      profopts.max_sample_pages = (int) tmp_val;
    }
  }
  printf("Maximum sample pages: %d\n", profopts.max_sample_pages);

  /* Get default_device_tag */
  env = getenv("SH_DEFAULT_NODE");
  default_device = NULL;
  if(env) {
    tmp_val = strtoimax(env, NULL, 10);
    default_device = get_device_from_numa_node((int) tmp_val);
  }
  if(!default_device) {
    /* This assumes that the normal page size is the first one that it'll find */
    default_device = get_device_from_numa_node(0);
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
      arenas_per_thread = num_numa_nodes; //(int) device_list.count;
      break;
    case SHARED_SITE_ARENAS:
    case EXCLUSIVE_SITE_ARENAS:
      arenas_per_thread = max_arenas;
      break;
    case EXCLUSIVE_TWO_DEVICE_ARENAS:
      arenas_per_thread = 2 * num_numa_nodes; //((int) device_list.count);
      break;
    case EXCLUSIVE_FOUR_DEVICE_ARENAS:
      arenas_per_thread = 4 * num_numa_nodes; //((int) device_list.count);
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
        sscanf(str, "%d", &site);
        str = strtok(NULL, " ");
        if(!str) {
          fprintf(stderr, "Read in a site number from the guidance file, but no node number. Aborting.\n");
          exit(1);
        }
        sscanf(str, "%d", &node);
        tree_insert(site_nodes, site, get_device_from_numa_node(node));
        printf("Adding site %d to NUMA node %d.\n", site, node);
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
      printf("Invalid number of static sites given.\n");
    } else {
      num_static_sites = (int) tmp_val;
    }
  }
  printf("Number of static sites: %d\n", num_static_sites);

  env = getenv("SH_RDSPY");
  profopts.should_run_rdspy = 0;
  if (env) {
    if (!num_static_sites) {
      printf("Invalid static sites -- not running rdspy.\n");
    }
    profopts.should_run_rdspy = 1 && num_static_sites;
    if (profopts.should_run_rdspy) {
      printf("Running with rdspy.\n");
    }
  }
}

__attribute__((constructor))
void sh_init() {
  int i;
  long size;

  device_list = sicm_init();

  /* Get the number of NUMA nodes with memory, since we ignore huge pages with
   * the DEVICE arena layouts */
  num_numa_nodes = 0;
  for(i = 0; i <= numa_max_node(); i++) {
    size = -1;
    if ((numa_node_size(i, &size) != -1) && size) {
      num_numa_nodes++;
    }
  }

  arena_counter = 0;
  site_arenas = tree_make(int, int);
  site_nodes = tree_make(int, deviceptr);
  device_arenas = tree_make(deviceptr, int);
  set_options();
  
  if(layout != INVALID_LAYOUT) {
    /* `arenas` is a pseudo-two-dimensional array, first dimension is per-thread.
     * Second dimension is one for each arena that each thread will have.
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
      case EXCLUSIVE_TWO_DEVICE_ARENAS:
      case EXCLUSIVE_FOUR_DEVICE_ARENAS:
        arenas = (arena_info **) calloc(max_threads * arenas_per_thread, sizeof(arena_info *));
        break;
    }

    /* Initialize the extents array.
     * If we're just doing MBI on one site, initialize a new array that has extents from just that site.
     * If we're profiling all sites, rss_extents is just all extents.
     */
    extents = extent_arr_init();
    if(profopts.should_profile_rss) {
      rss_extents = extents;
      if(profopts.should_profile_one) {
        rss_extents = extent_arr_init();
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
    pending_indices = (int *) malloc(max_threads * sizeof(int));
    for(i = 0; i < max_threads; i++) {
      pending_indices[i] = -1;
    }

    /* Set the arena allocator's callback function */
    sicm_extent_alloc_callback = &sh_create_extent;

    sh_start_profile_thread();
  }
  
  if (profopts.should_run_rdspy) {
    sh_rdspy_init(max_threads, num_static_sites);
  }
}

__attribute__((destructor))
void sh_terminate() {
  size_t i;

  /* Clean up the low-level interface */
  sicm_fini(&device_list);

  if(layout != INVALID_LAYOUT) {

    /* Clean up the profiler */
    if(profopts.should_profile_all || profopts.should_profile_one || profopts.should_profile_rss) {
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

  if (profopts.should_run_rdspy) {
      sh_rdspy_terminate();
  }
}
