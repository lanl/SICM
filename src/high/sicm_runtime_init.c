/* Required for dlsym */
#define _GNU_SOURCE
#include <dlfcn.h>
#include <stdbool.h>
#include <numa.h>
#include <sys/time.h>
#include <sys/resource.h>

#define SICM_RUNTIME 1
#include "sicm_runtime.h"
#include "sicm_rdspy.h"

/* Options for profiling */
profiling_options profopts = {0};

/* Keeps track of arenas, extents, etc. */
tracker_struct tracker = {0};

static atomic_int sh_being_initialized = 0;

/* Takes a string as input and outputs which arena layout it is */
enum arena_layout parse_layout(char *env) {
  size_t max_chars;

  max_chars = 32;

  if(strncmp(env, "ONE_ARENA", max_chars) == 0) {
    return ONE_ARENA;
  } else if(strncmp(env, "EXCLUSIVE_ARENAS", max_chars) == 0) {
    return EXCLUSIVE_ARENAS;
  } else if(strncmp(env, "EXCLUSIVE_DEVICE_ARENAS", max_chars) == 0) {
    return EXCLUSIVE_DEVICE_ARENAS;
  } else if(strncmp(env, "SHARED_SITE_ARENAS", max_chars) == 0) {
    return SHARED_SITE_ARENAS;
  } else if(strncmp(env, "BIG_SMALL_ARENAS", max_chars) == 0) {
    return BIG_SMALL_ARENAS;
  }

  return INVALID_LAYOUT;
}

/* Converts an arena_layout to a string */
char *layout_str(enum arena_layout layout) {
  switch(layout) {
    case ONE_ARENA:
      return "ONE_ARENA";
    case EXCLUSIVE_ARENAS:
      return "EXCLUSIVE_ARENAS";
    case EXCLUSIVE_DEVICE_ARENAS:
      return "EXCLUSIVE_DEVICE_ARENAS";
    case SHARED_SITE_ARENAS:
      return "SHARED_SITE_ARENAS";
    case BIG_SMALL_ARENAS:
      return "BIG_SMALL_ARENAS";
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

  if(retval == NULL) {
    fprintf(stderr, "Couldn't find an appropriate device for NUMA node %d, out of %lu devices.\n", id, tracker.device_list.count);
    exit(1);
  }

  return retval;
}

#define COMMON_OPTIONS \
  X(SH_LOG_FILE) \
  X(SH_ARENA_LAYOUT) \
  X(SH_BIG_SMALL_THRESHOLD) \
  X(SH_MAX_THREADS) \
  X(SH_MAX_ARENAS) \
  X(SH_MAX_SITES_PER_ARENA) \
  X(SH_MAX_SITES) \
  X(SH_UPPER_NODE) \
  X(SH_LOWER_NODE) \
  X(SH_DEFAULT_NODE)
  
#define PROFILE_OPTIONS \
  X(SH_PROFILE_INPUT_FILE) \
  X(SH_PROFILE_OUTPUT_FILE) \
  X(SH_PRINT_PROFILE_INTERVALS) \
  X(SH_PROFILE_ONLINE) \
  X(SH_PROFILE_ONLINE_DEBUG_FILE) \
  X(SH_PROFILE_ONLINE_RESERVED_BYTES) \
  X(SH_PROFILE_ONLINE_GRACE_ACCESSES) \
  X(SH_PROFILE_ONLINE_NOBIND) \
  X(SH_PROFILE_ONLINE_SKIP_INTERVALS) \
  X(SH_PROFILE_ONLINE_VALUE) \
  X(SH_PROFILE_ONLINE_WEIGHT) \
  X(SH_PROFILE_ONLINE_SORT) \
  X(SH_PROFILE_ONLINE_VALUE_THRESHOLD) \
  X(SH_PROFILE_ONLINE_PACKING_ALGO) \
  X(SH_PROFILE_ONLINE_USE_LAST_INTERVAL) \
  X(SH_PROFILE_ONLINE_LAST_ITER_VALUE) \
  X(SH_PROFILE_ONLINE_LAST_ITER_WEIGHT) \
  X(SH_PROFILE_ONLINE_STRAT_ORIG) \
  X(SH_PROFILE_ONLINE_RECONF_WEIGHT_RATIO) \
  X(SH_PROFILE_ONLINE_HOT_INTERVALS) \
  X(SH_PROFILE_ONLINE_STRAT_SKI) \
  X(SH_PROFILE_RATE_NSECONDS) \
  X(SH_PROFILE_NODES) \
  X(SH_PROFILE_ALL) \
  X(SH_PROFILE_ALL_EVENTS) \
  X(SH_PROFILE_ALL_MULTIPLIERS) \
  X(SH_PROFILE_ALL_SKIP_INTERVALS) \
  X(SH_PROFILE_IMC) \
  X(SH_PROFILE_BW) \
  X(SH_PROFILE_BW_SKIP_INTERVALS) \
  X(SH_PROFILE_BW_EVENTS) \
  X(SH_PROFILE_BW_RELATIVE) \
  X(SH_PROFILE_LATENCY) \
  X(SH_PROFILE_LATENCY_SET_MULTIPLIERS) \
  X(SH_PROFILE_LATENCY_SKIP_INTERVALS) \
  X(SH_PROFILE_LATENCY_EVENTS) \
  X(SH_PROFILE_LATENCY_CLOCKTICK_EVENT) \
  X(SH_PROFILE_RSS) \
  X(SH_PROFILE_RSS_SKIP_INTERVALS) \
  X(SH_PROFILE_EXTENT_SIZE) \
  X(SH_PROFILE_EXTENT_SIZE_SKIP_INTERVALS) \
  X(SH_PROFILE_ALLOCS) \
  X(SH_PROFILE_ALLOCS_SKIP_INTERVALS) \
  X(SH_SAMPLE_FREQ) \
  X(SH_MAX_SAMPLE_PAGES) \
  X(SH_NUM_STATIC_SITES) \
  X(SH_RDSPY)

#define X(name) char * name;
  COMMON_OPTIONS
  PROFILE_OPTIONS
#undef X

void print_options() {
  char *env;
  
  if(!tracker.log_file) {
    return;
  }
  
  fprintf(tracker.log_file, "===== OPTIONS =====\n");
  #define X(name) env = getenv(#name); if(env) fprintf(tracker.log_file, #name " = %s\n", env);
    COMMON_OPTIONS
    PROFILE_OPTIONS
  #undef X
  fprintf(tracker.log_file, "===== END OPTIONS =====\n");
  fflush(tracker.log_file);
  fclose(tracker.log_file);
}

void set_common_options() {
  char *env;
  long long tmp_val;
  
  /* Output the chosen options to this file */
  env = getenv("SH_LOG_FILE");
  tracker.log_file = NULL;
  if(env) {
    tracker.log_file = fopen(env, "w");
    if(!tracker.log_file) {
      fprintf(stderr, "Failed to open the specified logfile: '%s'. Aborting.\n", env);
      exit(1);
    }
  }
  
  /* Get the arena layout */
  env = getenv("SH_ARENA_LAYOUT");
  if(env) {
    tracker.layout = parse_layout(env);
  } else {
    tracker.layout = DEFAULT_ARENA_LAYOUT;
  }

  /* Get the threshold for a "big" or "small" arena, in bytes */
  if(tracker.layout == BIG_SMALL_ARENAS) {
    env = getenv("SH_BIG_SMALL_THRESHOLD");
    if(env) {
      tracker.big_small_threshold = strtoul(env, NULL, 0);
    }
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
   * Keep in mind that 4095 is the maximum number supported by jemalloc
   * (12 bits to store the arena IDs, minus one).
   * An error occurs if this limit is reached.
   */
  tracker.max_arenas = 4095;
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
  
  /* Get max_sites.
     This is the maximum number of allocation sites that you can have. Keep in mind
     that we use site IDs as indices into an array, so the maximum site ID that you can
     have is `tracker.max_sites - 1`. */
  tracker.max_sites = 4096;
  env = getenv("SH_MAX_SITES");
  if(env) {
    tmp_val = strtoimax(env, NULL, 10);
    if((tmp_val == 0) || (tmp_val > INT_MAX)) {
      fprintf(stderr, "Invalid arena number given. Aborting.\n");
      exit(1);
    } else {
      tracker.max_sites = (int) tmp_val;
    }
  }
  
  /* Get the devices */
  env = getenv("SH_UPPER_NODE");
  tracker.upper_device = NULL;
  if(env) {
    tmp_val = strtoimax(env, NULL, 10);
    tracker.upper_device = get_device_from_numa_node((int) tmp_val);
  }
  if(!tracker.upper_device) {
    fprintf(stderr, "WARNING: Upper device defaulting to NUMA node 0.\n");
    tracker.upper_device = get_device_from_numa_node(0);
  }
  env = getenv("SH_LOWER_NODE");
  tracker.lower_device = NULL;
  if(env) {
    tmp_val = strtoimax(env, NULL, 10);
    tracker.lower_device = get_device_from_numa_node((int) tmp_val);
  }
  if(!tracker.lower_device) {
    fprintf(stderr, "WARNING: Lower device defaulting to NUMA node 0.\n");
    tracker.lower_device = get_device_from_numa_node(0);
  }
  env = getenv("SH_DEFAULT_NODE");
  tracker.default_device = NULL;
  if(env) {
    tmp_val = strtoimax(env, NULL, 10);
    tracker.default_device = get_device_from_numa_node((int) tmp_val);
  }
  if(!tracker.default_device) {
    tracker.default_device = tracker.lower_device;
  }
}

void set_guided_options() {
  char *env, *line, guidance, found_guidance, *str;
  int node, site;
  FILE *guidance_file;
  ssize_t len;
  
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

        /* Use the arrays of atomics to set this site to go to the proper device */
        tracker.site_devices[site] = (atomic_int *) get_device_from_numa_node(node);
        
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
}

void set_profile_options() {
  char *env, *str;
  long long tmp_val;
  deviceptr device;
  size_t i, n;
  int node, cpu, site, num_cpus, num_nodes;
  ssize_t len;
  struct bitmask *cpus, *nodes;
  char flag;
  
  /* See if there's profiling information that we can use later */
  env = getenv("SH_PROFILE_INPUT_FILE");
  profopts.profile_input_file = NULL;
  if(env) {
    profopts.profile_input_file = fopen(env, "r");
    if(!profopts.profile_input_file) {
      fprintf(stderr, "Failed to open profile input file. Aborting.\n");
      exit(1);
    }
  }

  env = getenv("SH_PROFILE_OUTPUT_FILE");
  profopts.profile_output_file = NULL;
  if(env) {
    profopts.profile_output_file = fopen(env, "w");
    if(!profopts.profile_output_file) {
      fprintf(stderr, "Failed to open profile output file. Aborting.\n");
      exit(1);
    }
  }

  /* Should we generate and attempt to use per-interval profiling information? */
  env = getenv("SH_PRINT_PROFILE_INTERVALS");
  profopts.print_profile_intervals = 0;
  if(env) {
    profopts.print_profile_intervals = 1;
  }

  /* Do we want to use the online approach, moving arenas around devices automatically? */
  env = getenv("SH_PROFILE_ONLINE");
  profopts.profile_online_skip_intervals = 0;
  if(env) {
    enable_profile_online();

    env = getenv("SH_PROFILE_ONLINE_DEBUG_FILE");
    profopts.profile_online_debug_file = NULL;
    if(env) {
      profopts.profile_online_debug_file = fopen(env, "w");
      if(!profopts.profile_online_debug_file) {
        fprintf(stderr, "Failed to open profile_online debug file. Aborting.\n");
        exit(1);
      }
    }
    
    /* Grace period at the beginning of a run. Until this number of profiling accesses is reached,
       the profile_online won't rebind any sites. */
    env = getenv("SH_PROFILE_ONLINE_GRACE_ACCESSES");
    profopts.profile_online_grace_accesses = 0;
    if(env) {
      profopts.profile_online_grace_accesses = strtoul(env, NULL, 0);
    }

    /* Purely to measure the overhead of the online approach without doing any special binding */
    env = getenv("SH_PROFILE_ONLINE_NOBIND");
    profopts.profile_online_nobind = 0;
    if(env) {
      profopts.profile_online_nobind = 1;
    }

    env = getenv("SH_PROFILE_ONLINE_SKIP_INTERVALS");
    profopts.profile_online_skip_intervals = 1;
    if(env) {
      profopts.profile_online_skip_intervals = strtoul(env, NULL, 0);
    }
    
    env = getenv("SH_PROFILE_ONLINE_VALUE");
    if(env) {
      profopts.profile_online_value = orig_malloc((strlen(env) + 1) * sizeof(char));
      strcpy(profopts.profile_online_value, env);
    }
    
    env = getenv("SH_PROFILE_ONLINE_WEIGHT");
    if(env) {
      profopts.profile_online_weight = orig_malloc((strlen(env) + 1) * sizeof(char));
      strcpy(profopts.profile_online_weight, env);
    }

    env = getenv("SH_PROFILE_ONLINE_SORT");
    if(env) {
      profopts.profile_online_sort = orig_malloc((strlen(env) + 1) * sizeof(char));
      strcpy(profopts.profile_online_sort, env);
    }
    
    env = getenv("SH_PROFILE_ONLINE_PACKING_ALGO");
    if(env) {
      profopts.profile_online_packing_algo = orig_malloc((strlen(env) + 1) * sizeof(char));
      strcpy(profopts.profile_online_packing_algo, env);
    }
    
    env = getenv("SH_PROFILE_ONLINE_USE_LAST_INTERVAL");
    profopts.profile_online_use_last_interval = 0;
    if(env) {
      profopts.profile_online_use_last_interval = 1;
    }

    env = getenv("SH_PROFILE_ONLINE_LAST_ITER_VALUE");
    profopts.profile_online_last_iter_value = 0.0;
    if(env) {
      profopts.profile_online_last_iter_value = strtof(env, NULL);
    }

    env = getenv("SH_PROFILE_ONLINE_LAST_ITER_WEIGHT");
    profopts.profile_online_last_iter_weight = 0.0;
    if(env) {
      profopts.profile_online_last_iter_weight = strtof(env, NULL);
    }

    env = getenv("SH_PROFILE_ONLINE_STRAT_ORIG");
    profopts.profile_online_orig = 0;
    if(env) {
      profopts.profile_online_orig = 1;
    }
    
    env = getenv("SH_PROFILE_ONLINE_VALUE_THRESHOLD");
    profopts.profile_online_value_threshold = 0;
    if(env) {
      profopts.profile_online_value_threshold = strtoul(env, NULL, 0);
    }

    if(profopts.profile_online_orig) {
      /* Orig-specific configuration */

      env = getenv("SH_PROFILE_ONLINE_RECONF_WEIGHT_RATIO");
      profopts.profile_online_reconf_weight_ratio = 0.0;
      if(env) {
        profopts.profile_online_reconf_weight_ratio = strtof(env, NULL);
      }

      env = getenv("SH_PROFILE_ONLINE_HOT_INTERVALS");
      profopts.profile_online_hot_intervals = 0;
      if(env) {
        profopts.profile_online_hot_intervals = strtoul(env, NULL, 0);
      }
    }

    env = getenv("SH_PROFILE_ONLINE_STRAT_SKI");
    profopts.profile_online_ski = 0;
    if(env) {
      profopts.profile_online_ski = 1;
    }
  }


  /* Controls the profiling rate of all profiling types */
  profopts.profile_rate_nseconds = 0;
  env = getenv("SH_PROFILE_RATE_NSECONDS");
  if(env) {
    profopts.profile_rate_nseconds = strtoimax(env, NULL, 10);
  }

  /* The user can specify a number of NUMA nodes to profile on. For profile_all,
     all CPUs on the node(s) will be profiled. For profile_bw, one CPU from
     each node will be chosen to record the bandwidth on the IMC. */
  env = getenv("SH_PROFILE_NODES");
  profopts.num_profile_all_cpus = 0;
  profopts.num_profile_skt_cpus = 0;
  profopts.profile_all_cpus = NULL;
  profopts.profile_skt_cpus = NULL;
  profopts.profile_skts = NULL;
  if(env) {
    /* First, get a list of nodes that the user specified */
    nodes = numa_parse_nodestring(env);
    cpus = numa_allocate_cpumask();
    num_cpus = numa_num_configured_cpus();
    num_nodes = numa_num_configured_nodes();
    /* Iterate over the nodes in the `nodes` bitmask */
    for(node = 0; node < num_nodes; node++) {
      if(numa_bitmask_isbitset(nodes, node)) {
        numa_bitmask_clearall(cpus);
        numa_node_to_cpus(node, cpus);
        flag = 0;
        /* Now iterate over the CPUs on those nodes */
        for(cpu = 0; cpu < num_cpus; cpu++) {
          if(numa_bitmask_isbitset(cpus, cpu)) {
            /* Here, we'll add one (1) CPU from this list to profile_skt_cpus */
            if(!flag) {
              profopts.num_profile_skt_cpus++;
              profopts.profile_skt_cpus = orig_realloc(profopts.profile_skt_cpus,
                                                      sizeof(int) * profopts.num_profile_skt_cpus);
              profopts.profile_skts = orig_realloc(profopts.profile_skt_cpus,
                                                       sizeof(int) * profopts.num_profile_skt_cpus);
              profopts.profile_skt_cpus[profopts.num_profile_skt_cpus - 1] = cpu;
              profopts.profile_skts[profopts.num_profile_skt_cpus - 1] = node;
              flag = 1;
            }
            /* ...and add all of the CPUs to profile_all */
            profopts.num_profile_all_cpus++;
            profopts.profile_all_cpus = orig_realloc(profopts.profile_all_cpus, sizeof(int) * profopts.num_profile_all_cpus);
            profopts.profile_all_cpus[profopts.num_profile_all_cpus - 1] = cpu;
          }
        }
      }
    }
  } else {
    /* If the user doesn't set this, default to using the CPUs on the NUMA nodes that this
        process is allowed on */
    cpus = numa_all_cpus_ptr;
    num_cpus = numa_num_configured_cpus();
    flag = 0;
    for(cpu = 0; cpu < num_cpus; cpu++) {
      if(numa_bitmask_isbitset(cpus, cpu)) {
        if(!flag) {
          profopts.num_profile_skt_cpus++;
          profopts.profile_skt_cpus = orig_realloc(profopts.profile_skt_cpus,
                                                  sizeof(int) * profopts.num_profile_skt_cpus);
          profopts.profile_skts = orig_realloc(profopts.profile_skts,
                                                  sizeof(int) * profopts.num_profile_skt_cpus);
          profopts.profile_skt_cpus[profopts.num_profile_skt_cpus - 1] = cpu;
          profopts.profile_skts[profopts.num_profile_skt_cpus - 1] = numa_node_of_cpu(cpu);
          flag = 1;
        }
        profopts.num_profile_all_cpus++;
        profopts.profile_all_cpus = orig_realloc(profopts.profile_all_cpus, sizeof(int) * profopts.num_profile_all_cpus);
        profopts.profile_all_cpus[profopts.num_profile_all_cpus - 1] = cpu;
      }
    }
  }

  /* Should we profile all allocation sites using sampling-based profiling? */
  env = getenv("SH_PROFILE_ALL");
  if(env) {
    enable_profile_all();
  }
  if(should_profile_all()) {

    env = getenv("SH_PROFILE_ALL_EVENTS");
    profopts.num_profile_all_events = 0;
    profopts.profile_all_events = NULL;
    if(env) {
      /* Parse out the events into an array */
      while((str = strtok(env, ",")) != NULL) {
        profopts.num_profile_all_events++;
        profopts.profile_all_events = orig_realloc(profopts.profile_all_events, sizeof(char *) * profopts.num_profile_all_events);
        profopts.profile_all_events[profopts.num_profile_all_events - 1] = orig_malloc(sizeof(char) * (strlen(str) + 1));
        strcpy(profopts.profile_all_events[profopts.num_profile_all_events - 1], str);
        env = NULL;
      }
    }
    if(profopts.num_profile_all_events == 0) {
      fprintf(stderr, "No profiling events given. Can't profile with sampling.\n");
      exit(1);
    }
    
    env = getenv("SH_PROFILE_ALL_MULTIPLIERS");
    profopts.num_profile_all_multipliers = 0;
    profopts.profile_all_multipliers = NULL;
    if(env) {
      /* Parse out the events into an array */
      while((str = strtok(env, ",")) != NULL) {
        profopts.num_profile_all_multipliers++;
        profopts.profile_all_multipliers = orig_realloc(profopts.profile_all_multipliers, sizeof(float) * profopts.num_profile_all_multipliers);
        profopts.profile_all_multipliers[profopts.num_profile_all_multipliers - 1] = strtof(str, NULL);
        env = NULL;
      }
      if(profopts.num_profile_all_multipliers != profopts.num_profile_all_events) {
        fprintf(stderr, "Number of multipliers doesn't equal the number of PROFILE_ALL events. Aborting.\n");
        exit(1);
      }
    }

    env = getenv("SH_PROFILE_ALL_SKIP_INTERVALS");
    profopts.profile_all_skip_intervals = 1;
    if(env) {
      profopts.profile_all_skip_intervals = strtoul(env, NULL, 0);
    }
  }

  /* Should we keep track of when each allocation happened, in intervals? */
  env = getenv("SH_PROFILE_ALLOCS");
  if(env) {
    enable_profile_allocs();
  }
  
  /* The user should specify a comma-delimited list of IMCs to read the
    * bandwidth from. This will be passed to libpfm. For example, on an Ivy
    * Bridge server, this value is e.g. `ivbep_unc_imc0`, and on KNL it's
    * `knl_unc_imc0`. These are per-socket IMCs, so they will be used on all
    * sockets that you choose to profile on.
    */
  env = getenv("SH_PROFILE_IMC");
  profopts.num_imcs = 0;
  profopts.imcs = NULL;
  if(env) {
    /* Parse out the IMCs into an array */
    while((str = strtok(env, ",")) != NULL) {
      profopts.num_imcs++;
      profopts.imcs = orig_realloc(profopts.imcs, sizeof(char *) * profopts.num_imcs);
      profopts.imcs[profopts.num_imcs - 1] = str;
      env = NULL;
    }
  }
  
  /* Should we profile bandwidth on a specific socket? */
  env = getenv("SH_PROFILE_BW");
  profopts.profile_bw_skip_intervals = 0;
  if(env) {
    enable_profile_bw();
    
    if(profopts.num_imcs == 0) {
      fprintf(stderr, "No IMCs given. Can't enable profile_bw.\n");
      exit(1);
    }

    env = getenv("SH_PROFILE_BW_SKIP_INTERVALS");
    profopts.profile_bw_skip_intervals = 1;
    if(env) {
      profopts.profile_bw_skip_intervals = strtoul(env, NULL, 0);
    }
    
    /* The user should specify a number of CPUs to use to read
       the bandwidth from the IMCs. In the case of many machines,
       this usually means that the user should select one CPU per socket. */
    
    /*
    What events should be used to measure the bandwidth?
     */
    env = getenv("SH_PROFILE_BW_EVENTS");
    profopts.num_profile_bw_events = 0;
    profopts.profile_bw_events = NULL;
    if(env) {
      /* Parse out the events into an array */
      while((str = strtok(env, ",")) != NULL) {
        profopts.num_profile_bw_events++;
        profopts.profile_bw_events = orig_realloc(profopts.profile_bw_events, sizeof(char *) * profopts.num_profile_bw_events);
        profopts.profile_bw_events[profopts.num_profile_bw_events - 1] = orig_malloc(sizeof(char) * (strlen(str) + 1));
        strcpy(profopts.profile_bw_events[profopts.num_profile_bw_events - 1], str);
        env = NULL;
      }
    }
    if(profopts.num_profile_bw_events == 0) {
      fprintf(stderr, "No profiling events given. Can't profile one arena.\n");
      exit(1);
    }
    
    /* Should we try to associate bandwidth with arenas, using their
       relative profile_all values? */
    env = getenv("SH_PROFILE_BW_RELATIVE");
    profopts.profile_bw_relative = 0;
    if(env) {
      profopts.profile_bw_relative = 1;
    }
  }
  
  /* Should we profile latency on a specific socket? */
  env = getenv("SH_PROFILE_LATENCY");
  profopts.profile_latency_skip_intervals = 0;
  if(env) {
    enable_profile_latency();
    
    if(profopts.num_imcs == 0) {
      fprintf(stderr, "No IMCs given. Can't enable profile_latency.\n");
      exit(1);
    }
    
    env = getenv("SH_PROFILE_LATENCY_SET_MULTIPLIERS");
    profopts.profile_latency_set_multipliers = 0;
    if(env) {
      profopts.profile_latency_set_multipliers = 1;
    }

    env = getenv("SH_PROFILE_LATENCY_SKIP_INTERVALS");
    profopts.profile_latency_skip_intervals = 1;
    if(env) {
      profopts.profile_latency_skip_intervals = strtoul(env, NULL, 0);
    }
    
    /*
      What events should be used to measure the latency?
    */
    env = getenv("SH_PROFILE_LATENCY_EVENTS");
    profopts.num_profile_latency_events = 0;
    profopts.profile_latency_events = NULL;
    if(env) {
      /* Parse out the events into an array */
      while((str = strtok(env, ",")) != NULL) {
        profopts.num_profile_latency_events++;
        profopts.profile_latency_events = orig_realloc(profopts.profile_latency_events, sizeof(char *) * profopts.num_profile_latency_events);
        profopts.profile_latency_events[profopts.num_profile_latency_events - 1] = orig_malloc(sizeof(char) * (strlen(str) + 1));
        strcpy(profopts.profile_latency_events[profopts.num_profile_latency_events - 1], str);
        env = NULL;
      }
      if(profopts.num_profile_latency_events != 8) {
        fprintf(stderr, "Currently, the profile_latency profiler hardcodes a metric composed of exactly eight (read and write) events.\n");
        fprintf(stderr, "The first four should be for the upper tier, and the last four should be for the lower tier.\n");
        fprintf(stderr, "Aborting.\n");
        exit(1);
      }
    }
    if(profopts.num_profile_latency_events == 0) {
      fprintf(stderr, "No profiling events given. Can't profile latency.\n");
      exit(1);
    }
    
    /*
      We'll need an event to measure the DRAM clockticks
    */
    env = getenv("SH_PROFILE_LATENCY_CLOCKTICK_EVENT");
    profopts.profile_latency_clocktick_event = NULL;
    if(env) {
      profopts.profile_latency_clocktick_event = orig_malloc(sizeof(char) * (strlen(env) + 1));
      strcpy(profopts.profile_latency_clocktick_event, env);
    }
    if(profopts.profile_latency_clocktick_event == NULL) {
      fprintf(stderr, "Need an event to get clockticks from the DRAM. Can't profile latency. Aborting.\n");
      exit(1);
    }
  }

  /* Should we get the RSS of each arena? */
  env = getenv("SH_PROFILE_RSS");
  profopts.profile_rss_skip_intervals = 0;
  if(env) {
    enable_profile_rss();

    env = getenv("SH_PROFILE_RSS_SKIP_INTERVALS");
    profopts.profile_rss_skip_intervals = 1;
    if(env) {
      profopts.profile_rss_skip_intervals = strtoul(env, NULL, 0);
    }
  }

  env = getenv("SH_PROFILE_EXTENT_SIZE");
  if(env) {
    enable_profile_extent_size();

    env = getenv("SH_PROFILE_EXTENT_SIZE_SKIP_INTERVALS");
    profopts.profile_extent_size_skip_intervals = 1;
    if(env) {
      profopts.profile_extent_size_skip_intervals = strtoul(env, NULL, 0);
    }
  }

  env = getenv("SH_PROFILE_ALLOCS");
  if(env) {
    enable_profile_allocs();

    env = getenv("SH_PROFILE_ALLOCS_SKIP_INTERVALS");
    profopts.profile_allocs_skip_intervals = 1;
    if(env) {
      profopts.profile_allocs_skip_intervals = strtoul(env, NULL, 0);
    }
  }

  env = getenv("SH_PROFILE_OBJMAP");
  if(env) {
    enable_profile_objmap();

    env = getenv("SH_PROFILE_OBJMAP_SKIP_INTERVALS");
    profopts.profile_objmap_skip_intervals = 1;
    if(env) {
      profopts.profile_objmap_skip_intervals = strtoul(env, NULL, 0);
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
    if((tmp_val <= 0) || (tmp_val > 2048) || (tmp_val & (tmp_val - 1))) {
      fprintf(stderr, "Invalid number of pages given. Aborting.\n");
      exit(1);
    } else {
      profopts.max_sample_pages = (int) tmp_val;
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
  Dl_info dlinfo;
  void *handle;
  bool on;
  int err;
  struct rlimit rlim;
  char *s;

  if(atomic_fetch_add(&sh_being_initialized, 1) > 0) {
    fprintf(stderr, "sh_init called more than once: %d\n", sh_being_initialized);
    fflush(stderr);
    return;
  }
  
  rlim.rlim_cur = -1;
  rlim.rlim_max = -1;
  setrlimit(RLIMIT_STACK, &rlim);
  
  /* Disable the background thread in jemalloc to avoid a segfault */
  on = false;
  err = je_mallctl("background_thread", NULL, NULL, (void *)&on, sizeof(bool));
  if(err) {
    fprintf(stderr, "Failed to disable background threads: %d\n", err);
    exit(1);
  }
  
  handle = dlopen("libc.so.6", RTLD_LAZY);
  if(handle) {
    orig_malloc_ptr = dlsym(handle, "malloc");
    orig_valloc_ptr = dlsym(handle, "valloc");
    orig_calloc_ptr = dlsym(handle, "calloc");
    orig_realloc_ptr = dlsym(handle, "realloc");
    orig_free_ptr = dlsym(handle, "free");
    if((!orig_malloc_ptr) ||
       (!orig_valloc_ptr) ||
       (!orig_calloc_ptr) ||
       (!orig_realloc_ptr) ||
       (!orig_free_ptr)) {
         fprintf(stderr, "Failed to acquire the allocation symbols from libc. Aborting.\n");
         exit(1);
    }
    if(!dladdr(orig_malloc_ptr, &dlinfo)) {
      fprintf(stderr, "dladdr failed. Aborting.\n");
      exit(1);
    }
    if(!dladdr(orig_realloc_ptr, &dlinfo)) {
      fprintf(stderr, "dladdr failed. Aborting.\n");
      exit(1);
    }
  } else {
    fprintf(stderr, "%s\n", dlerror());
    exit(1);
  }

  tracker.device_list = sicm_init();

  /* Initialize all of the locks */
  pthread_rwlock_init(&tracker.extents_lock, NULL);
  pthread_mutex_init(&tracker.arena_lock, NULL);
  pthread_rwlock_init(&tracker.device_arenas_lock, NULL);

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
  
  set_common_options();
  
  /* Get arenas_per_thread */
  switch(tracker.layout) {
    case ONE_ARENA:
      tracker.arenas_per_thread = 1;
      break;
    case EXCLUSIVE_ARENAS:
      tracker.arenas_per_thread = 1;
      break;
    case EXCLUSIVE_DEVICE_ARENAS:
      tracker.arenas_per_thread = 2;
      break;
    case SHARED_SITE_ARENAS:
    case BIG_SMALL_ARENAS:
    default:
      tracker.arenas_per_thread = 1;
      break;
  };


  if(tracker.layout != INVALID_LAYOUT) {
    tracker.arenas = (arena_info **) orig_calloc(tracker.max_arenas, sizeof(arena_info *));
    
    /* These atomics keep track of per-site information, such as:
       1. `site_bigs`: boolean for if the site is above big_small_threshold or not.
       2. `site_sizes`: the number of bytes allocated to this site.
       3. `site_devices`: a pointer to the device that this site should be bound to.
       4. `site_arenas`: an integer index of the arena this site gets allocated to.
    */
    tracker.site_bigs = (atomic_char *) orig_malloc((tracker.max_sites + 1) * sizeof(atomic_char));
    for(i = 0; i < tracker.max_sites + 1; i++) {
      tracker.site_bigs[i] = -1;
    }
    tracker.site_sizes = (atomic_size_t *) orig_calloc(tracker.max_sites + 1, sizeof(atomic_size_t));
    tracker.site_devices = (atomic_int **) orig_malloc((tracker.max_sites + 1) * sizeof(atomic_int *));
    for(i = 0; i < tracker.max_sites + 1; i++) {
      tracker.site_devices[i] = (atomic_int *) NULL;
    }
    tracker.site_arenas = (atomic_int *) orig_malloc((tracker.max_sites + 1) * sizeof(atomic_int));
    for(i = 0; i < tracker.max_sites + 1; i++) {
      tracker.site_arenas[i] = -1;
    }

    /* This is just an atomic counter that we use to grab a new
       index for every thread that allocates for the first time. */
    tracker.current_thread_index = 0;
    
    /* Initialize the extents array.
     */
    tracker.extents = extent_arr_init();
  }
  
  set_profile_options();
  set_guided_options();
  
  if(should_profile()) {
    /* Set the arena allocator's callback function */
    sicm_extent_alloc_callback = &sh_create_extent;
    sicm_extent_dalloc_callback = &sh_delete_extent;
  }

  if(should_profile()) {
    sh_start_profile_master_thread();
  }

  if (profopts.should_run_rdspy) {
    sh_rdspy_init(tracker.max_threads, tracker.num_static_sites);
  }
  
  print_options();
  
  /* Re-enable the background thread in jemalloc to avoid a segfault */
  on = true;
  err = je_mallctl("background_thread", NULL, NULL, (void *)&on, sizeof(bool));
  if(err) {
    fprintf(stderr, "Failed to re-enable background threads: %d\n", err);
    exit(1);
  }

  sh_initialized = 1;
}

static pthread_once_t sh_term = PTHREAD_ONCE_INIT;
void sh_terminate_helper() {
  size_t i, n;
  arena_info *arena;
  int err;
  bool on;

  if(!sh_initialized) {
    return;
  }
  sh_initialized = 0;
  
  printf("SICM used %zu bytes of memory.\n", sicm_mem_usage);
  
  /* We need to stop the profiling first */
  if(tracker.layout != INVALID_LAYOUT) {
    if(should_profile()) {
      sh_stop_profile_master_thread();
    }
  }
    
  /* Now disable background threads in jemalloc, so we don't get any allocations
     happening while we're terminating */
  on = false;
  err = je_mallctl("background_thread", NULL, NULL, (void *)&on, sizeof(bool));
  if(err) {
    fprintf(stderr, "Failed to disable background threads: %d\n", err);
    exit(1);
  }

  /* Now that no allocations are happening, we can clean up the arenas */
  if(tracker.layout != INVALID_LAYOUT) {
    /* Clean up the arenas */
    arena_arr_for(i) {
      arena_check_good(arena, i);
      if(tracker.arenas[i]->arena) {
        sicm_arena_destroy(tracker.arenas[i]->arena);
      }
      orig_free(tracker.arenas[i]);
    }
    orig_free(tracker.arenas);
    extent_arr_free(tracker.extents);
  }

  /* Clean up the low-level interface */
  sicm_fini(&tracker.device_list);

  if(profopts.should_run_rdspy) {
    sh_rdspy_terminate();
  }
}

__attribute__((destructor))
void sh_terminate() {
  pthread_once(&sh_term, sh_terminate_helper);
}
