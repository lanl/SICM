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

/* Include the various online implementations */
#include "sicm_profile_online_utils.h"
#include "sicm_profile_online_orig.h"
#include "sicm_profile_online_ski.h"

/* Helper functions called by the application, for debugging purposes. */
#include "sicm_helpers.h"

static int phase_changes = 0;

void sh_profile_online_phase_change() {
  if(should_profile_online()) {
    profile_online_info *online;
    online = get_profile_online_prof();
    online->phase_change = 1;
    phase_changes++;
  }
}

void profile_online_arena_init(per_arena_profile_online_info *);
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
  size_t invalid_weight, hotset_weight, diff;

  /* Look at how much the application has consumed on each tier */
  prof.profile_online.upper_avail = sicm_avail(tracker.upper_device) * 1024;
  prof.profile_online.lower_avail = sicm_avail(tracker.lower_device) * 1024;
  
  if(prof.profile_online.using_sorted_sites && prof.profile_online.using_hotset) {
    /* In this condition, we'll make sure that the online approach isn't overpacking. */
    
    /* Here, we'll figure out the total (new) capacity of the current hotset */
    hotset_weight = 0;
    tree_traverse(prof.profile_online.using_hotset, hit) {
      hotset_weight += get_arena_objmap_prof(tree_it_val(hit)->index)->current_present_bytes;
    }
    get_profile_online_prof()->using_hotset_weight = hotset_weight;
    
    if(hotset_weight > prof.profile->upper_capacity) {
      /* Here, we're overpacking. Try to fix it by rebinding without the coldest site(s). */
      diff = hotset_weight - prof.profile->upper_capacity;
      //printf("We're overpacking by %zu bytes (%zu/%zu)\n", diff, get_profile_objmap_prof()->cgroup_node0_current, get_profile_objmap_prof()->cgroup_node0_max);
    } else {
      diff = prof.profile->upper_capacity - hotset_weight;
      //printf("We're NOT overpacking by %zu bytes (%zu/%zu)\n", diff, get_profile_objmap_prof()->cgroup_node0_current, get_profile_objmap_prof()->cgroup_node0_max);
    }
    //fflush(stdout);
    
    //full_rebind_down(prof.profile_online.using_sorted_sites);
    //full_rebind_first(prof.profile_online.using_sorted_sites);
    //get_profile_online_prof()->reconfigure = 1;
  } else {
    get_profile_online_prof()->using_hotset_weight = 0;
  }
  
  prof.profile_online.first_upper_contention = 0;
  if((prof.profile_online.lower_avail < prof.profile->lower_capacity) && (!(prof.profile_online.upper_contention))) {
    /* If the lower tier is being used, we're going to assume that the
       upper tier is under contention. Trip a flag. */
    prof.profile_online.upper_contention = 1;
    prof.profile_online.first_upper_contention = 1;
    if(profopts.profile_online_debug_file) {
      fprintf(profopts.profile_online_debug_file, "First upper contention.\n");
      fflush(profopts.profile_online_debug_file);
    }
  }
  
  /* Convert to a tree of sites */
  sorted_sites = sh_convert_to_site_tree(prof.profile, SIZE_MAX, &invalid_weight);

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
                            prof.profile->upper_capacity - profopts.profile_online_reserved_bytes,
                            invalid_weight);
  hotset_weight = 0;
  tree_traverse(merged_sorted_sites, sit) {
    hit = tree_lookup(hotset, tree_it_val(sit));
    if(tree_it_good(hit)) {
      get_arena_online_prof(tree_it_key(sit)->index)->hot = 1;
      hotset_weight += get_arena_objmap_prof(tree_it_val(hit)->index)->current_present_bytes;
    } else {
      get_arena_online_prof(tree_it_key(sit)->index)->hot = 0;
    }
  }
  if(profopts.profile_online_debug_file) {
    fprintf(profopts.profile_online_debug_file,
            "Current hotset weight: %zu\n", hotset_weight);
  }
  hotset_weight = 0;
  tree_traverse(merged_sorted_sites, sit) {
    hit = tree_lookup(hotset, tree_it_val(sit));
    if(tree_it_good(hit)) {
      hotset_weight += get_arena_objmap_prof(tree_it_val(hit)->index)->peak_present_bytes;
    }
  }
  if(profopts.profile_online_debug_file) {
    fprintf(profopts.profile_online_debug_file,
            "Peak hotset weight: %zu\n", hotset_weight);
  }
  
  if(prof.profile_online.first_online_interval == 1) {
    /* We've already had our first interval with the online approach having taken over,
       so now indicate that we're in our second or greater interval. */
    prof.profile_online.first_online_interval = 2;
  }
  
  /* Using the `first_upper_contention` is crucial here: we only want to trigger this if
     this is the very first time that the online approach has noticed contention for the upper tier, AND
     when we've got enough profiling information to make an informed decision. */
  if(prof.profile_online.first_upper_contention) {
    if((get_profile_all_prof()->total > profopts.profile_online_value_threshold)) {
      /* This is when the online approach actually takes over. Default all subsequent allocations to the lower tier.
        Bind all sites to the lower tier, until the chosen online strategy decides to bind them up. */
      tracker.default_device = tracker.lower_device;
      prof.profile_online.first_online_interval = 1;
      
      if(profopts.profile_online_debug_file) {
        fprintf(profopts.profile_online_debug_file, "Online approach taking over because the total is: %zu.\n", get_profile_all_prof()->total);
      }
    } else {
      /* We're not quite ready to do our first rebind yet. Reset and try again. */
      prof.profile_online.upper_contention = 0;
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
void profile_online_arena_init(per_arena_profile_online_info *info) {
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
}

void profile_online_init() {
  size_t i, n;
  char found;
  application_profile *offline_profile;
  packing_options *opts;
  
  opts = orig_calloc(sizeof(char), sizeof(packing_options));
  
  /* Let the user choose these, but sh_packing_init will set defaults if not */
  if(profopts.profile_online_value) {
    opts->value = sh_packing_value_flag(profopts.profile_online_value);
  }
  if(profopts.profile_online_weight) {
    opts->weight = sh_packing_weight_flag(profopts.profile_online_weight);
  }
  if(profopts.profile_online_packing_algo) {
    opts->algo = sh_packing_algo_flag(profopts.profile_online_packing_algo);
  }
  if(profopts.profile_online_sort) {
    opts->sort = sh_packing_sort_flag(profopts.profile_online_sort);
  }
  if(profopts.profile_online_debug_file) {
    opts->debug_file = profopts.profile_online_debug_file;
  }
  
  /* The previous and current profiling *need* to have the same type of profiling for this
     to make sense. Otherwise, you're just going to get errors. */
  offline_profile = NULL;
  prof.profile_online.offline_sorted_sites = NULL;
  prof.profile_online.offline_invalid_weight = 0;
  if(profopts.profile_input_file) {
    offline_profile = sh_parse_profiling(profopts.profile_input_file);
    sh_packing_init(offline_profile, &opts);
    prof.profile_online.offline_sorted_sites = sh_convert_to_site_tree(offline_profile, offline_profile->num_intervals - 1, &(prof.profile_online.offline_invalid_weight));
  } else {
    sh_packing_init(prof.profile, &opts);
  }

  /* Figure out the amount of free memory that we're starting out with */
  prof.profile->upper_capacity = (size_t) get_cgroup_node0_max();
  if(!(prof.profile->upper_capacity)) {
    prof.profile->upper_capacity = sicm_avail(tracker.upper_device) * 1024;
  }
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

  prof.profile_online.upper_contention = 0;
  prof.profile_online.first_online_interval = 0;
  prof.profile_online.using_sorted_sites = NULL;

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
}
