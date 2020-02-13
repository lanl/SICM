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

/* At the beginning of an interval, keeps track of stats and figures out what
   should happen during rebind. */
tree(site_info_ptr, int) prepare_stats() {
  size_t upper_avail, lower_avail;
  char dev, hot, prev_hot;
  int index;

  /* Trees and iterators to interface with the parsing/packing libraries */
  tree(site_info_ptr, int) sorted_sites;
  tree(site_info_ptr, int) merged_sorted_sites;
  tree(int, site_info_ptr) hotset;
  tree_it(int, site_info_ptr) hit;
  tree_it(site_info_ptr, int) sit;

  /* Look at how much the application has consumed on each tier */
  upper_avail = sicm_avail(tracker.upper_device) * 1024;
  lower_avail = sicm_avail(tracker.lower_device) * 1024;

  if(lower_avail < prof.profile_online.lower_avail_initial && (!prof.profile_online.upper_contention)) {
    /* If the lower tier is being used, we're going to assume that the
       upper tier is under contention. Trip a flag and let the online
       approach take over. Begin defaulting all new allocations to the lower
       tier. */
    prof.profile_online.upper_contention = 1;
    tracker.default_device = tracker.lower_device;
  }

  /* Convert to a tree of sites and generate the new hotset */
  sorted_sites = sh_convert_to_site_tree(prof.profile,
                                         prof.profile->num_intervals - 1);
  if(prof.profile_online.offline_sorted_sites) {
    /* If we have a previous run's profiling, take that into account */
    merged_sorted_sites = sh_merge_site_trees(prof.profile_online.offline_sorted_sites,
                                              sorted_sites,
                                              profopts.profile_online_last_iter_value,
                                              profopts.profile_online_last_iter_weight);
  } else {
    merged_sorted_sites = sorted_sites;
  }

  /* Calculate the hotset, then mark each arena's hotness */
  hotset = sh_get_hot_sites(merged_sorted_sites,
                            prof.profile_online.upper_avail_initial);
  tree_traverse(merged_sorted_sites, sit) {
    hit = tree_lookup(hotset, tree_it_val(sit));
    if(tree_it_good(hit)) {
      get_arena_online_prof(tree_it_key(sit)->index)->hot = 1;
    } else {
      get_arena_online_prof(tree_it_key(sit)->index)->hot = 0;
    }
  }

  /* Calculate the stats */
  prof.profile_online.total_site_weight     = 0;
  prof.profile_online.total_site_value      = 0;
  prof.profile_online.total_sites           = 0;
  prof.profile_online.site_weight_diff      = 0;
  prof.profile_online.site_value_diff       = 0;
  prof.profile_online.num_sites_diff        = 0;
  prof.profile_online.site_weight_to_rebind = 0;
  prof.profile_online.site_value_to_rebind  = 0;
  prof.profile_online.num_sites_to_rebind   = 0;
  tree_traverse(merged_sorted_sites, sit) {
    index = tree_it_key(sit)->index;
    dev = get_arena_online_prof(index)->dev;
    hot = get_arena_online_prof(index)->hot;
    prev_hot = get_prev_arena_online_prof(index)->hot;

    prof.profile_online.total_site_weight += tree_it_key(sit)->weight;
    prof.profile_online.total_site_value += tree_it_key(sit)->value;
    prof.profile_online.total_sites++;

    if(hot) {
      get_arena_online_prof(index)->num_hot_intervals++;
    } else {
      get_arena_online_prof(index)->num_hot_intervals = 0;
    }

    /* Differences between hotsets */
    if((hot && !prev_hot) ||
       (!hot && prev_hot)) {
      /* The site will be rebound if a full rebind happens */
      prof.profile_online.site_weight_diff += tree_it_key(sit)->weight;
      prof.profile_online.site_value_diff += tree_it_key(sit)->value;
      prof.profile_online.num_sites_diff++;
    }

    /* Calculate what would have to be rebound if the current hotset
       were to trigger a full rebind */
    if(((dev == -1) && hot) ||
       ((dev == 0)  && hot) ||
       ((dev == 1)  && !hot)) {
        /* If the site is in the hotset, but not in the upper tier, OR
           if the site is not in the hotset, but in the upper tier */
        prof.profile_online.site_weight_to_rebind += tree_it_key(sit)->weight;
        prof.profile_online.site_value_to_rebind += tree_it_key(sit)->value;
        prof.profile_online.num_sites_to_rebind++;
    }
  }

  /* Free everything up */
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
  arena_info *arena;
  arena_profile *aprof;
  sicm_dev_ptr dl;
  int retval;
  char full_rebind, dev, hot;

  tree(site_info_ptr, int) sorted_sites;
  tree_it(site_info_ptr, int) sit;

  sorted_sites = prepare_stats();

  full_rebind = 0;
  if(!profopts.profile_online_nobind &&
     prof.profile_online.upper_contention &&
     (prof.profile_online.total_site_value > profopts.profile_online_grace_accesses) &&
     ((((float) prof.profile_online.site_weight_to_rebind) / ((float) prof.profile_online.total_site_weight)) >= profopts.profile_online_reconf_weight_ratio)) {
    /* Do a full rebind. Take the difference between what's currently on the devices (site_tiers),
       and what the hotset says should be on there. */
    tree_traverse(sorted_sites, sit) {
      index = tree_it_key(sit)->index;
      dev = get_arena_online_prof(index)->dev;
      hot = get_arena_online_prof(index)->hot;
      prev_hot = get_prev_arena_online_prof(index)->hot;

      dl = NULL;
      if(((dev == -1) && hot) ||
         ((dev == 0) && hot)) {
        /* The site is in AEP, and is in the hotset. */
        dl = prof.profile_online.upper_dl;
        get_arena_online_prof(index)->dev = 1;
      } else if((dev == 1) && (hot == 0)) {
        /* The site is in DRAM and isn't in the hotset */
        dl = prof.profile_online.lower_dl;
        get_arena_online_prof(index)->dev = 0;
      }

      if(dl) {
        full_rebind = 1;
        retval = sicm_arena_set_devices(tracker.arenas[index]->arena, dl);
        if(retval == -EINVAL) {
          fprintf(stderr, "Rebinding arena %d failed in SICM.\n", index);
        } else if(retval != 0) {
          fprintf(stderr, "Rebinding arena %d failed internally.\n", index);
        }
      }
    }
  } else {
    /* No full rebind, but we can bind specific sites if the conditions are right */
    if(profopts.profile_online_hot_intervals) {
      /* If the user specified a number of intervals, rebind the sites that
         have been hot for that amount of intervals */
      tree_traverse(sorted_sites, sit) {
        index = tree_it_key(sit)->index;
        num_hot_intervals = get_arena_online_prof(index)->num_hot_intervals;
        if(num_hot_intervals == profopts.profile_online_hot_intervals) {
          get_arena_online_prof(index)->dev = 1;
          retval = sicm_arena_set_devices(tracker.arenas[index]->arena,
                                          prof.profile_online.upper_dl);
          if(retval == -EINVAL) {
            fprintf(stderr, "Rebinding arena %d failed in SICM.\n", index);
          } else if(retval != 0) {
            fprintf(stderr, "Rebinding arena %d failed internally.\n", index);
          }
        }
      }
    }
  }

  tree_traverse(sorted_sites, sit) {
    if(tree_it_key(sit)) {
      orig_free(tree_it_key(sit));
    }
  }
  tree_free(sorted_sites);

  /* Maintain the previous hotset */
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
                    &profopts.profile_online_events,
                    &profopts.num_profile_online_events,
                    &weight,
                    &algo,
                    &sort,
                    profopts.profile_online_weights,
                    profopts.profile_online_debug);
    prof.profile_online.offline_sorted_sites = sh_convert_to_site_tree(offline_profile, offline_profile->num_intervals - 1);
  } else {
    sh_packing_init(prof.profile,
                    &value,
                    &profopts.profile_online_events,
                    &profopts.num_profile_online_events,
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

  prof.profile_online.num_reconfigures = 0;
  prof.profile_online.upper_contention = 0;
}

void profile_online_deinit() {
}

void profile_online_post_interval(arena_profile *info) {
}

void profile_online_skip_interval(int s) {
  end_interval();
}
