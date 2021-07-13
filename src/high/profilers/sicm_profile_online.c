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
#include "sicm_profile_online_ski.h"

/* Helper functions called by the application, for debugging purposes. */
#include "sicm_helpers.h"

static int phase_changes = 0;

void sh_profile_online_phase_change() {
  if(should_profile_online()) {
    get_online_prof()->phase_change = 1;
    phase_changes++;
    if(profopts.profile_online_debug_file) {
      fprintf(profopts.profile_online_debug_file, "Phase change.\n");
      fflush(profopts.profile_online_debug_file);
    }
  }
}

/* Generates a hotset, then returns a tree of *all* sites, with the hot ones marked */
tree(site_info_ptr, int) calculate_hotset() {
  tree(site_info_ptr, int) online_sites;
  tree(site_info_ptr, int) merged_sites;
  tree(int, site_info_ptr) hotset;
  tree_it(int, site_info_ptr) hit;
  tree_it(site_info_ptr, int) sit;
  size_t site_invalid_weight, invalid_weight, internal_usage, i, n;
  
  internal_usage = peak_internal_present_usage(profopts.profile_online_debug_file) * prof.profile_pebs.pagesize;
  #if 0
  if(profopts.profile_online_debug_file) {
    for(n = 0; n < profopts.num_profile_pebs_cpus; n++) {
      for(i = 0; i < prof.profile->num_profile_pebs_events; i++) {
        fprintf(profopts.profile_online_debug_file, "PEBS buffer: %p\n", prof.profile_pebs.metadata[n][i]);
      }
    }
  }
  fflush(profopts.profile_online_debug_file);
  print_smaps_info();
  
  /* We want to add all that we know of to `invalid_weight`, so that the packing algorithm
     doesn't accidentally overpack if there's a lot of other data in the upper tier. */
  if(profopts.profile_online_debug_file) {
    fprintf(profopts.profile_online_debug_file, "Internal present bytes: %zu\n", internal_usage);
    fflush(profopts.profile_online_debug_file);
  }
  #endif
  invalid_weight = internal_usage;
  invalid_weight += (profopts.num_profile_pebs_cpus *
                    prof.profile->num_profile_pebs_events *
                    (prof.profile_pebs.pagesize + (prof.profile_pebs.pagesize * profopts.max_sample_pages)));
  
  /* Get the sorted sites, taking into account a possible offline profile
     Please note that here, we're taking into account peak SICM memory usage, which
     is the peak usage that we've used in our `internal_` functions for internal allocations. */
  site_invalid_weight = 0;
  online_sites = sh_convert_to_site_tree(prof.profile, SIZE_MAX, &site_invalid_weight);
  invalid_weight += site_invalid_weight;
  if(profopts.profile_online_debug_file) {
    fprintf(profopts.profile_online_debug_file, "Invalid site bytes: %zu\n", site_invalid_weight);
    fflush(profopts.profile_online_debug_file);
  }
  if(get_online_data()->offline_sorted_sites) {
    /* If we have a previous run's profiling, take that into account */
    merged_sites = sh_merge_site_trees(get_online_data()->offline_sorted_sites,
                                              online_sites,
                                              profopts.profile_online_last_iter_value,
                                              profopts.profile_online_last_iter_weight);
  } else {
    /* If there's no offline profile, we can just copy `online_sites` to `merged_sites`. */
    merged_sites = copy_site_tree(online_sites);
  }
  if(profopts.profile_online_debug_file) {
    fprintf(profopts.profile_online_debug_file, "%zu bytes are defaulting to the upper tier.\n", invalid_weight);
    fflush(profopts.profile_online_debug_file);
  }
  
  /* From the sorted sites, generate a hotset and mark them in the tree */
  hotset = sh_get_hot_sites(merged_sites,
                            get_online_data()->upper_max,
                            invalid_weight);
  tree_traverse(merged_sites, sit) {
    hit = tree_lookup(hotset, tree_it_val(sit));
    if(tree_it_good(hit)) {
      get_online_arena_prof(tree_it_key(sit)->index)->hot = 1;
    } else {
      get_online_arena_prof(tree_it_key(sit)->index)->hot = 0;
    }
    get_online_arena_prof(tree_it_key(sit)->index)->weight = tree_it_key(sit)->weight;
  }
  
  /* Clean up */
  free_site_tree(online_sites);
  
  return merged_sites;
}

void profile_online_arena_init(per_arena_profile_online_info *);
void profile_online_deinit();
void profile_online_init();
void *profile_online(void *);
void profile_online_interval(int);
void profile_online_skip_interval(int);
void profile_online_post_interval(arena_profile *);

/* Initializes the profiling information for one arena for one interval */
void profile_online_arena_init(per_arena_profile_online_info *info) {
  info->dev = -1;
  info->hot = -1;
  info->weight = 0;
}

void *profile_online(void *a) {
  pthread_setcanceltype(PTHREAD_CANCEL_ASYNCHRONOUS, NULL);

  while(1) { }
}

void profile_online_interval(int s) {
  tree(site_info_ptr, int) merged_sites;
  tree_it(site_info_ptr, int) sit;
  
  if(profopts.profile_online_debug_file) {
    fprintf(profopts.profile_online_debug_file, "===== BEGIN INTERVAL %zu.\n", prof.profile->num_intervals);
    fflush(profopts.profile_online_debug_file);
  }
  
  /* Determine when the lower tier begins being consumed */
  get_online_data()->upper_max = get_cgroup_node0_max();
  get_online_data()->lower_used = get_cgroup_node1_current();
  get_online_data()->upper_used = get_cgroup_node0_current();
  get_online_prof()->phase_change = 0;
  get_online_prof()->reconfigure = 0;

  /* In this block, we'll determine:
     1. upper_contention: whether or not the upper tier is contended for.
     2. first_upper_contention: whether or not this is the first contended interval.
     3. first_online_interval: whether or not we've had an online interval yet. */
  get_online_data()->first_upper_contention = 0;
  if(get_online_data()->lower_used && (!(get_online_data()->upper_contention))) {
    /* If the lower tier is being used, we're going to assume that the
       upper tier is under contention. Trip a flag. */
    get_online_data()->upper_contention = 1;
    get_online_data()->first_upper_contention = 1;
  }
  if(get_online_data()->first_online_interval == 1) {
    /* We've already had our first interval with the online approach having taken over,
       so now indicate that we're in our second or greater interval. */
    get_online_data()->first_online_interval = 2;
  }
  if(get_online_data()->first_upper_contention) {
    if((get_pebs_prof()->total > profopts.profile_online_value_threshold)) {
      tracker.default_device = tracker.lower_device;
      get_online_data()->first_online_interval = 1;
    } else {
      get_online_data()->upper_contention = 0;
    }
  }
  merged_sites = calculate_hotset();
  prepare_stats_ski(merged_sites);
  profile_online_interval_ski(merged_sites);
  
  if(profopts.profile_online_debug_file) {
    fprintf(profopts.profile_online_debug_file,
            "Upper tier: %llu / %llu\n", get_cgroup_node0_current(), get_cgroup_node0_max());
    fprintf(profopts.profile_online_debug_file, "===== END INTERVAL %zu.\n", prof.profile->num_intervals);
    fflush(profopts.profile_online_debug_file);
  }
  
  /* Clean up */
  free_site_tree(merged_sites);
}

void profile_online_init() {
  size_t i, n;
  char found;
  application_profile *offline_profile;
  packing_options *opts;
  
  init_thread_suspension();
  
  opts = internal_calloc(sizeof(char), sizeof(packing_options));
  
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
  opts->alpha = profopts.profile_online_alpha;
  
  /* The previous and current profiling *need* to have the same type of profiling for this
     to make sense. Otherwise, you're just going to get errors. */
  offline_profile = NULL;
  get_online_data()->offline_sorted_sites = NULL;
  get_online_data()->offline_invalid_weight = 0;
  if(profopts.profile_input_file) {
    offline_profile = sh_parse_profiling(profopts.profile_input_file);
    sh_packing_init(offline_profile, &opts);
    get_online_data()->offline_sorted_sites = sh_convert_to_site_tree(offline_profile, offline_profile->num_intervals - 1, &(get_online_data()->offline_invalid_weight));
  } else {
    sh_packing_init(prof.profile, &opts);
  }

  /* Since sicm_arena_set_devices accepts a device_list, construct these */
  get_online_data()->upper_dl = internal_malloc(sizeof(struct sicm_device_list));
  get_online_data()->upper_dl->count = 1;
  get_online_data()->upper_dl->devices = internal_malloc(sizeof(deviceptr));
  get_online_data()->upper_dl->devices[0] = tracker.upper_device;
  get_online_data()->lower_dl = internal_malloc(sizeof(struct sicm_device_list));
  get_online_data()->lower_dl->count = 1;
  get_online_data()->lower_dl->devices = internal_malloc(sizeof(deviceptr));
  get_online_data()->lower_dl->devices[0] = tracker.lower_device;

  get_online_data()->upper_contention = 0;
  get_online_data()->first_online_interval = 0;
  get_online_data()->prev_bw = 0;
  get_online_data()->cur_bw = 0;
  get_online_data()->prev_interval_reconfigure = 0;
  
  get_online_data()->cur_sorted_sites = NULL;
  get_online_data()->prev_sorted_sites = NULL;

  get_online_data()->ski = internal_malloc(sizeof(profile_online_data_ski));
}

void profile_online_deinit() {
}

void profile_online_post_interval(arena_profile *info) {
}

void profile_online_skip_interval(int s) {
  get_online_prof()->phase_change = 0;
  get_online_prof()->reconfigure = 0;
}
