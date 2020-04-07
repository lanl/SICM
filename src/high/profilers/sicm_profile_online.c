#include <sys/stat.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <unistd.h>
#include <sys/syscall.h>
#include <errno.h>
#include <sys/types.h>
#include <time.h>

#define SICM_RUNTIME 1
#include "sicm_runtime.h"
#include "sicm_profilers.h"
#include "sicm_profile.h"
#include "sicm_packing.h"

/* Include the various online implementations */
#include "sicm_profile_online_utils.h"
#include "sicm_profile_online_orig.h"
#include "sicm_profile_online_ski.h"

void profile_online_arena_init(profile_online_info *);
void profile_online_deinit();
void profile_online_init();
void *profile_online(void *);
void profile_online_interval(int);
void profile_online_skip_interval(int);
void profile_online_post_interval(arena_profile *);

/* At the beginning of an interval, keeps track of stats and figures out what
   should happen during rebind. */
tree(site_info_ptr, int) prepare_stats() {

  /* Trees and iterators to interface with the parsing/packing libraries */
  tree(site_info_ptr, int) sorted_sites;
  tree(site_info_ptr, int) merged_sorted_sites;
  tree(int, site_info_ptr) hotset;
  tree_it(int, site_info_ptr) hit;
  tree_it(site_info_ptr, int) sit;

  /* Look at how much the application has consumed on each tier */
  prof.profile_online.upper_avail = sicm_avail(tracker.upper_device) * 1024;
  prof.profile_online.lower_avail = sicm_avail(tracker.lower_device) * 1024;

  if((prof.profile_online.lower_avail < prof.profile->lower_capacity) && (!prof.profile_online.upper_contention)) {
    /* If the lower tier is being used, we're going to assume that the
       upper tier is under contention. Trip a flag and let the online
       approach take over. Begin defaulting all new allocations to the lower
       tier. */
    prof.profile_online.upper_contention = 1;
    tracker.default_device = tracker.lower_device;
    sorted_sites = sh_convert_to_site_tree(prof.profile, SIZE_MAX);
    full_rebind_cold(sorted_sites);
  }

  /* Convert to a tree of sites */
  sorted_sites = sh_convert_to_site_tree(prof.profile, SIZE_MAX);

  /* If we've got offline profiling, use it */
  if(prof.profile_online.offline_sorted_sites) {
    /* If we have a previous run's profiling, take that into account */
    merged_sorted_sites = sh_merge_site_trees(prof.profile_online.offline_sorted_sites,
                                              sorted_sites,
                                              profopts.profile_online_last_iter_value,
                                              profopts.profile_online_last_iter_weight);
  } else {
    merged_sorted_sites = sorted_sites;
  }

  /* Calculate the hotset, then mark each arena's hotness
     in the profiling so that it'll be recorded for this interval */
  hotset = sh_get_hot_sites(merged_sorted_sites,
                            prof.profile->upper_capacity);
  tree_traverse(merged_sorted_sites, sit) {
    hit = tree_lookup(hotset, tree_it_val(sit));
    if(tree_it_good(hit)) {
      get_arena_online_prof(tree_it_key(sit)->index)->hot = 1;
    } else {
      get_arena_online_prof(tree_it_key(sit)->index)->hot = 0;
    }
  }

  /* Free up the offline profile, but not the online one */
  if(prof.profile_online.offline_sorted_sites) {
    tree_traverse(sorted_sites, sit) {
      if(tree_it_key(sit)) {
        orig_free(tree_it_key(sit));
      }
    }
    tree_free(sorted_sites);
  }

  return merged_sorted_sites;
}

/* Initializes the profiling information for one arena for one interval */
void profile_online_arena_init(profile_online_info *info) {
  info->dev = -1;
  info->hot = -1;
  info->num_hot_intervals = 0;
}

void *profile_online(void *a) {
  pthread_setcanceltype(PTHREAD_CANCEL_ASYNCHRONOUS, NULL);

  while(1) { }
}

void profile_online_interval(int s) {
  tree(site_info_ptr, int) sorted_sites;
  tree_it(site_info_ptr, int) sit;

  /* Call the appropriate strategy */
  sorted_sites = prepare_stats();
  if(profopts.profile_online_ski) {
    prepare_stats_ski(sorted_sites);
    profile_online_interval_ski(sorted_sites);
  } else {
    /* Default to the original strategy */
    prepare_stats_orig(sorted_sites);
    profile_online_interval_orig(sorted_sites);
  }

  /* Free up what we allocated */
  tree_traverse(sorted_sites, sit) {
    if(tree_it_key(sit)) {
      orig_free(tree_it_key(sit));
    }
  }
  tree_free(sorted_sites);

  end_interval();
}

void profile_online_init() {
  size_t i, n;
  char found;
  application_profile *offline_profile;
  packing_options *opts;
  
  opts = orig_calloc(sizeof(char), sizeof(packing_options));
  
  if(profopts.should_profile_all &&
     profopts.should_profile_bw &&
     profopts.profile_bw_relative) {
    opts->value = PROFILE_BW_RELATIVE_TOTAL;
  } else {
    fprintf(stderr, "The online approach currently requires both PROFILE_ALL and PROFILE_BW to work. Aborting.\n");
    exit(1);
  }
  
  /* Determine which type of profiling to use to determine weight. Error if none found. */
  if(profopts.should_profile_allocs) {
    opts->weight = PROFILE_ALLOCS_PEAK;
  } else if(profopts.should_profile_extent_size) {
    opts->weight = PROFILE_EXTENT_SIZE_PEAK;
  } else if(profopts.should_profile_rss) {
    opts->weight = PROFILE_RSS_PEAK;
  } else {
    fprintf(stderr, "The online approach requires some kind of capacity profiling. Aborting.\n");
    exit(1);
  }
  
  if(strcmp(profopts.profile_online_packing_algo, "hotset") == 0) {
    opts->algo = HOTSET;
  } else if(strcmp(profopts.profile_online_packing_algo, "thermos") == 0) {
    opts->algo = THERMOS;
  }
  
  opts->sort = VALUE_PER_WEIGHT;
  
  /* The previous and current profiling *need* to have the same type of profiling for this
     to make sense. Otherwise, you're just going to get errors. */
  offline_profile = NULL;
  prof.profile_online.offline_sorted_sites = NULL;
  if(profopts.profile_input_file) {
    offline_profile = sh_parse_profiling(profopts.profile_input_file);
    sh_packing_init(offline_profile, &opts);
    prof.profile_online.offline_sorted_sites = sh_convert_to_site_tree(offline_profile, offline_profile->num_intervals - 1);
  } else {
    sh_packing_init(prof.profile, &opts);
  }

  /* Figure out the amount of free memory that we're starting out with */
  prof.profile->upper_capacity = sicm_avail(tracker.upper_device) * 1024;
  prof.profile->lower_capacity = sicm_avail(tracker.lower_device) * 1024;

  /* Since sicm_arena_set_devices accepts a device_list, construct these */
  prof.profile_online.upper_dl = orig_malloc(sizeof(struct sicm_device_list));
  prof.profile_online.upper_dl->count = 1;
  prof.profile_online.upper_dl->devices = orig_malloc(sizeof(deviceptr));
  prof.profile_online.upper_dl->devices[0] = tracker.upper_device;
  prof.profile_online.lower_dl = orig_malloc(sizeof(struct sicm_device_list));
  prof.profile_online.lower_dl->count = 1;
  prof.profile_online.lower_dl->devices = orig_malloc(sizeof(deviceptr));
  prof.profile_online.lower_dl->devices[0] = tracker.lower_device;

  prof.profile_online.num_reconfigures = 0;
  prof.profile_online.upper_contention = 0;

  /* Initialize the strategy-specific stuff */
  if(profopts.profile_online_orig) {
    profile_online_init_orig();
  } else if(profopts.profile_online_ski) {
    profile_online_init_ski();
  }
}

void profile_online_deinit() {
}

void profile_online_post_interval(arena_profile *info) {
}

void profile_online_skip_interval(int s) {
  end_interval();
}
