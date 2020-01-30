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

void profile_online_arena_init(profile_online_info *);
void profile_online_deinit();
void profile_online_init();
void *profile_online(void *);
void profile_online_interval(int);
void profile_online_skip_interval(int);
void profile_online_post_interval(arena_profile *);

void profile_online_arena_init(profile_online_info *info) {
}

void *profile_online(void *a) {
  pthread_setcanceltype(PTHREAD_CANCEL_ASYNCHRONOUS, NULL);

  while(1) { }
}

void profile_online_interval(int s) {
  size_t upper_avail, lower_avail,
         i, n;
  arena_info *arena;
  arena_profile *aprof;
  sicm_dev_ptr dl;
  int retval;

  /* Iterators for the trees in the profile_online_data struct */
  tree_it(int, sicm_dev_ptr) tit;
  tree_it(int, size_t) hit;

  /* Stats */
  size_t total_site_weight, site_weight_to_rebind,
         total_site_value, site_value_to_rebind,
         total_sites, num_sites_to_rebind;
  char full_rebind;

  /* Trees store site information, value is the site ID */
  tree(site_info_ptr, int) sorted_sites;
  tree(site_info_ptr, int) merged_sorted_sites;
  tree_it(site_info_ptr, int) sit;

  /* Store the sites as keys, values are structs with profiling info */
  tree(int, site_info_ptr) hotset;
  tree(int, site_info_ptr) prev_hotset;
  tree_it(int, site_info_ptr) old, new;

  if(profopts.profile_online_output_file) {
    /* Print out some initial debugging info */
    fprintf(profopts.profile_online_output_file, "===== BEGIN RECONFIGURE %d =====\n", prof.profile_online.num_reconfigures);
    fprintf(profopts.profile_online_output_file, "  Beginning timestamp: %ld\n", time(NULL));
  }

  /* Look at how much the application has consumed on each tier */
  upper_avail = sicm_avail(tracker.upper_device) * 1024;
  lower_avail = sicm_avail(tracker.lower_device) * 1024;
  if(lower_avail < prof.profile_online.lower_avail_initial && (!prof.profile_online.upper_contention)) {
    /* If the lower tier is being used, we're going to assume that the
       upper tier is under contention. Trip a flag and let the online
       approach take over. */
    prof.profile_online.upper_contention = 1;
    tracker.default_device = tracker.lower_device;
  }

  /* Maintain the previous hotset */
  if(!prof.profile_online.prev_hotset) {
    prof.profile_online.prev_hotset = (void *) tree_make(int, site_info_ptr);
  }
  prev_hotset = (tree(int, site_info_ptr)) prof.profile_online.prev_hotset;

  /* Convert to a tree of sites and generate the new hotset */
  sorted_sites = sh_convert_to_site_tree(prof.profile);
  if(prof.profile_online.offline_sorted_sites) {
    /* If we have a previous run's profiling, take that into account */
    merged_sorted_sites = sh_merge_site_trees(prof.profile_online.offline_sorted_sites, sorted_sites, profopts.profile_online_last_iter_value, profopts.profile_online_last_iter_weight);
  } else {
    merged_sorted_sites = sorted_sites;
  }
  hotset = sh_get_hot_sites(merged_sorted_sites, prof.profile_online.upper_avail_initial);

  /* Add up some stats by iterating over every site. */
  total_site_weight = 0;
  site_weight_to_rebind = 0;
  total_site_value = 0;
  site_value_to_rebind = 0;
  total_sites = 0;
  num_sites_to_rebind = 0;
  tree_traverse(merged_sorted_sites, sit) {
    old = tree_lookup(prev_hotset, tree_it_val(sit));
    new = tree_lookup(hotset, tree_it_val(sit));

    total_site_weight += tree_it_key(sit)->weight;
    total_site_value += tree_it_key(sit)->value;
    total_sites++;

    if(tree_it_good(new)) {
      /* Maintain the tree which stores the number of hot intervals that a site has had */
      hit = tree_lookup(prof.profile_online.site_hot_intervals, tree_it_val(sit));
      if(tree_it_good(hit)) {
        tree_insert(prof.profile_online.site_hot_intervals, tree_it_val(sit), tree_it_val(hit) + 1);
      } else {
        tree_insert(prof.profile_online.site_hot_intervals, tree_it_val(sit), 1);
      }
    } else {
      tree_insert(prof.profile_online.site_hot_intervals, tree_it_val(sit), 0);
    }

    if((tree_it_good(new) && !tree_it_good(old)) ||
       (!tree_it_good(new) && tree_it_good(old))) {
      /* The site will be rebound if a full rebind happens */
      site_weight_to_rebind += tree_it_key(sit)->weight;
      site_value_to_rebind += tree_it_key(sit)->value;
      num_sites_to_rebind++;
    }
  }

  full_rebind = 0;
  if((!profopts.profile_online_nobind) &&
     (prof.profile_online.upper_contention) &&
     (total_site_value > profopts.profile_online_grace_accesses) &&
     ((site_weight_to_rebind / total_site_weight) >= profopts.profile_online_reconf_weight_ratio)) {
    /* Do a full rebind */
    tree_traverse(merged_sorted_sites, sit) {
      old = tree_lookup(prev_hotset, tree_it_val(sit));
      new = tree_lookup(hotset, tree_it_val(sit));

      /* A site should only be rebound if it's:
        1. In the new hotset and not in the old.
        2. In the old hotset and not in the new. */
      dl = NULL;
      if(tree_it_good(new) && !tree_it_good(old)) {
        dl = prof.profile_online.upper_dl;
      } else if(!tree_it_good(new) && tree_it_good(old)) {
        dl = prof.profile_online.lower_dl;
      } else {
        tree_insert(prof.profile_online.site_tiers, tree_it_val(sit), prof.profile_online.lower_dl);
      }

      if(dl) {
        /* This only counts as a full rebind if a site is actually moved */
        full_rebind = 1;
        tree_insert(prof.profile_online.site_tiers, tree_it_val(sit), dl);
        retval = sicm_arena_set_devices(tracker.arenas[tree_it_key(sit)->index]->arena, dl);
        if(retval == -EINVAL) {
          fprintf(stderr, "Rebinding arena %d failed in SICM.\n", tree_it_key(sit)->index);
        } else if(retval != 0) {
          fprintf(stderr, "Rebinding arena %d failed internally.\n", tree_it_key(sit)->index);
        }
      }
    }
  } else {
    if(profopts.profile_online_hot_intervals) {
      /* If the user specified a number of intervals, rebind the sites that
         have been hot for that amount of intervals */
      tree_traverse(merged_sorted_sites, sit) {
        hit = tree_lookup(prof.profile_online.site_hot_intervals, tree_it_val(sit));
        if(tree_it_val(hit) == profopts.profile_online_hot_intervals) {
          retval = sicm_arena_set_devices(tracker.arenas[tree_it_key(sit)->index]->arena, prof.profile_online.upper_dl);
          if(retval == -EINVAL) {
            fprintf(stderr, "Rebinding arena %d failed in SICM.\n", tree_it_key(sit)->index);
          } else if(retval != 0) {
            fprintf(stderr, "Rebinding arena %d failed internally.\n", tree_it_key(sit)->index);
          }
        }
      }
    }
  }

  if(profopts.profile_online_output_file) {
    /* Print out as much debugging information as we can. */
    fprintf(profopts.profile_online_output_file, "  Ending timestamp: %ld\n", time(NULL));
    fprintf(profopts.profile_online_output_file, "  Upper avail: %zu\n", upper_avail);
    fprintf(profopts.profile_online_output_file, "  Lower avail: %zu\n", lower_avail);
    if(full_rebind) {
      fprintf(profopts.profile_online_output_file, "  Full rebind: yes\n");
    } else {
      fprintf(profopts.profile_online_output_file, "  Full rebind: no\n");
    }
    fprintf(profopts.profile_online_output_file, "  Weight rebound: %zu\n", site_weight_to_rebind);
    fprintf(profopts.profile_online_output_file, "  Value rebound: %zu\n", site_value_to_rebind);
    fprintf(profopts.profile_online_output_file, "  Hot sites: ");
    tree_traverse(hotset, new) {
      fprintf(profopts.profile_online_output_file, "%d ", tree_it_key(new));
    }
    fprintf(profopts.profile_online_output_file, "\n");
    fprintf(profopts.profile_online_output_file, "  DRAM sites: ");
    tree_traverse(prof.profile_online.site_tiers, tit) {
      if(tree_it_val(tit) == prof.profile_online.upper_dl) {
        fprintf(profopts.profile_online_output_file, "%d ", tree_it_key(tit));
      }
    }
    fprintf(profopts.profile_online_output_file, "\n");
    fprintf(profopts.profile_online_output_file, "  Sorted sites: ");
    tree_traverse(merged_sorted_sites, sit) {
      fprintf(profopts.profile_online_output_file, "%d ", tree_it_val(sit));
    }
    fprintf(profopts.profile_online_output_file, "\n");
    fprintf(profopts.profile_online_output_file, "  Values: ");
    tree_traverse(merged_sorted_sites, sit) {
      fprintf(profopts.profile_online_output_file, "%zu ", tree_it_key(sit)->value);
    }
    fprintf(profopts.profile_online_output_file, "\n");
    fprintf(profopts.profile_online_output_file, "  Weights: ");
    tree_traverse(merged_sorted_sites, sit) {
      fprintf(profopts.profile_online_output_file, "%zu ", tree_it_key(sit)->weight);
    }
    fprintf(profopts.profile_online_output_file, "\n");
    fprintf(profopts.profile_online_output_file, "  V/W: ");
    tree_traverse(merged_sorted_sites, sit) {
      fprintf(profopts.profile_online_output_file, "%lf ", tree_it_key(sit)->value_per_weight);
    }
    fprintf(profopts.profile_online_output_file, "\n");
    fprintf(profopts.profile_online_output_file, "===== END RECONFIGURE %d =====\n", prof.profile_online.num_reconfigures);
  }

  /* Free everything up */
  tree_free(prev_hotset);
  if(prof.profile_online.offline_sorted_sites) {
    tree_traverse(merged_sorted_sites, sit) {
      if(tree_it_key(sit)) {
        orig_free(tree_it_key(sit));
      }
    }
    tree_free(merged_sorted_sites);
  }
  tree_traverse(sorted_sites, sit) {
    if(tree_it_key(sit)) {
      orig_free(tree_it_key(sit));
    }
  }
  tree_free(sorted_sites);

  /* Maintain the previous hotset */
  prof.profile_online.prev_hotset = (void *) hotset;
  prof.profile_online.num_reconfigures++;

  end_interval();
}

void profile_online_init() {
  size_t i, n;
  char found;
  char *weight;
  char *value;
  char *algo;
  char *sort;
  application_profile *offline_profile;

  /* Determine which type of profiling to use to determine weight. Error if none found. */
  if(profopts.should_profile_allocs) {
    weight = malloc((strlen("profile_allocs") + 1) * sizeof(char));
    strcpy(weight, "profile_allocs");
  } else if(profopts.should_profile_extent_size) {
    weight = malloc((strlen("profile_extent_size") + 1) * sizeof(char));
    strcpy(weight, "profile_extent_size");
  } else if(profopts.should_profile_rss) {
    weight = malloc((strlen("profile_rss") + 1) * sizeof(char));
    strcpy(weight, "profile_rss");
  } else {
    fprintf(stderr, "The online approach requires some kind of capacity profiling. Aborting.\n");
    exit(1);
  }

  /* Look for the event that we're supposed to use for value. Error out if it's not found. */
  if(!profopts.should_profile_all) {
    fprintf(stderr, "SH_PROFILE_ONLINE requires SH_PROFILE_ALL. Aborting.\n");
    exit(1);
  }
  value = malloc((strlen("profile_all") + 1) * sizeof(char));
  strcpy(value, "profile_all");

  /* Find the event string and make sure that the event is available. */
  found = 0;
  for(i = 0; i < profopts.num_profile_online_events; i++) {
    for(n = 0; n < prof.profile->num_profile_all_events; n++) {
      if(strcmp(prof.profile->profile_all_events[n], profopts.profile_online_events[i]) == 0) {
        found++;
        break;
      }
    }
  }
  if(found != profopts.num_profile_online_events) {
    fprintf(stderr, "At least one of the events in SH_PROFILE_ONLINE_EVENTS wasn't found in SH_PROFILE_ALL_EVENTS. Aborting.\n");
    exit(1);
  }

  algo = orig_malloc((strlen("hotset") + 1) * sizeof(char));
  strcpy(algo, "hotset");
  sort = orig_malloc((strlen("value_per_weight") + 1) * sizeof(char));
  strcpy(sort, "value_per_weight");

  /* The previous and current profiling *need* to have the same type of profiling for this
     to make sense. Otherwise, you're just going to get errors. */
  offline_profile = NULL;
  prof.profile_online.offline_sorted_sites = NULL;
  if(profopts.profile_input_file) {
    offline_profile = sh_parse_profiling(profopts.profile_input_file);
    sh_packing_init(offline_profile,
                    &value,
                    profopts.profile_online_events,
                    profopts.num_profile_online_events,
                    &weight,
                    &algo,
                    &sort,
                    profopts.profile_online_weights,
                    profopts.profile_online_debug);
    prof.profile_online.offline_sorted_sites = sh_convert_to_site_tree(offline_profile);
  } else {
    sh_packing_init(prof.profile,
                    &value,
                    profopts.profile_online_events,
                    profopts.num_profile_online_events,
                    &weight,
                    &algo,
                    &sort,
                    profopts.profile_online_weights,
                    profopts.profile_online_debug);
  }

  /* Figure out the amount of free memory that we're starting out with */
  prof.profile_online.upper_avail_initial = sicm_avail(tracker.upper_device) * 1024;
  prof.profile_online.lower_avail_initial = sicm_avail(tracker.lower_device) * 1024;

  /* Since sicm_arena_set_devices accepts a device_list, construct these */
  prof.profile_online.upper_dl = orig_malloc(sizeof(struct sicm_device_list));
  prof.profile_online.upper_dl->count = 1;
  prof.profile_online.upper_dl->devices = orig_malloc(sizeof(deviceptr));
  prof.profile_online.upper_dl->devices[0] = tracker.upper_device;
  prof.profile_online.lower_dl = orig_malloc(sizeof(struct sicm_device_list));
  prof.profile_online.lower_dl->count = 1;
  prof.profile_online.lower_dl->devices = orig_malloc(sizeof(deviceptr));
  prof.profile_online.lower_dl->devices[0] = tracker.lower_device;

  prof.profile_online.prev_hotset = NULL;
  prof.profile_online.num_reconfigures = 0;
  prof.profile_online.upper_contention = 0;
  prof.profile_online.site_tiers = (void *) tree_make(int, sicm_dev_ptr);
  prof.profile_online.site_hot_intervals = (void *) tree_make(int, size_t);

  if(profopts.profile_online_output_file) {
    fprintf(profopts.profile_online_output_file, "Online init: %ld\n", time(NULL));
  }
}

void profile_online_deinit() {
  if(profopts.profile_online_output_file) {
    fprintf(profopts.profile_online_output_file, "Online deinit: %ld\n", time(NULL));
  }
}

void profile_online_post_interval(arena_profile *info) {
}

void profile_online_skip_interval(int s) {
  end_interval();
}
