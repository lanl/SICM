#include <sys/stat.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <unistd.h>
#include <sys/syscall.h>
#include <errno.h>
#include <sys/types.h>

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
  size_t i, n, upper_avail, lower_avail;
  arena_info *arena;
  arena_profile *aprof;
  struct sicm_device_list *dl;

  /* Trees store site information, value is the site ID */
  tree(site_info_ptr, int) sorted_sites;
  tree(site_info_ptr, int) merged_sorted_sites;
  tree_it(site_info_ptr, int) sit;

  /* Store the sites as keys, values are structs with profiling info */
  tree(int, site_info_ptr) hotset;
  tree(int, site_info_ptr) prev_hotset;
  tree_it(int, site_info_ptr) hit, old, new;

  /* Look at how much the application has consumed on each tier */
  upper_avail = sicm_avail(tracker.upper_device) * 1024;
  lower_avail = sicm_avail(tracker.lower_device) * 1024;

  if(lower_avail < prof.profile_online.lower_avail_initial) {
    /* The lower tier is now being used, so we need to reconfigure. */
    if(profopts.profile_online_print_reconfigures) {
      printf("Reconfigure %d\n", prof.profile_online.num_reconfigures);
    }

    /* If we don't make everything default to the lower device from now on,
       we'd have to continuously check and rebind every site when it pops
       into existence. */
    tracker.default_device = tracker.lower_device;

    /* If this is the first interval, the previous hotset was the empty set */
    if(!prof.profile_online.prev_hotset) {
      if(profopts.profile_online_print_reconfigures) {
        printf("There was no previous hotset, so making a blank one.\n");
      }
      prof.profile_online.prev_hotset = (void *) tree_make(int, site_info_ptr);
    }
    prev_hotset = (tree(int, site_info_ptr)) prof.profile_online.prev_hotset;

    /* Convert to a tree of sites and generate the new hotset */
    sorted_sites = sh_convert_to_site_tree(prof.profile);

    /* If we have a previous run's profiling, take that into account */
    if(prof.profile_online.last_iter_sorted_sites) {
      printf("Printing old sorted sites:\n");
      tree_traverse(prof.profile_online.last_iter_sorted_sites, sit) {
        printf("(%d, %zu) ", tree_it_val(sit), tree_it_key(sit)->value);
      }
      printf("\n");
      merged_sorted_sites = sh_merge_site_trees(prof.profile_online.last_iter_sorted_sites, sorted_sites, profopts.profile_online_last_iter_value, profopts.profile_online_last_iter_weight);
    } else {
      merged_sorted_sites = sorted_sites;
    }

    hotset = sh_get_hot_sites(merged_sorted_sites, prof.profile_online.upper_avail_initial);

    if(profopts.profile_online_print_reconfigures) {
      printf("Previous hotset: ");
      tree_traverse(prev_hotset, hit) {
        printf("%d ", tree_it_key(hit));
      }
      printf("\n");

      printf("Current hotset: ");
      tree_traverse(hotset, hit) {
        printf("%d ", tree_it_key(hit));
      }
      printf("\n");
    }

    if(!profopts.profile_online_nobind) {
      /* Iterate over all of the sites. Rebind if:
         1. A site wasn't in the previous hotset, but is in the current one.
         2. A site was in the previous hotset, but now isn't. */
      tree_traverse(sorted_sites, sit) {
        /* Look to see if it's in the new or old hotsets. */
        old = tree_lookup(prev_hotset, tree_it_val(sit));
        new = tree_lookup(hotset, tree_it_val(sit));
        dl = NULL;
        if(tree_it_good(new) && !tree_it_good(old)) {
          dl = prof.profile_online.upper_dl;
          if(profopts.profile_online_print_reconfigures) {
            printf("Binding site %d to the upper tier.\n", tree_it_val(sit));
          }
        } else if(!tree_it_good(new) && tree_it_good(old)) {
          dl = prof.profile_online.lower_dl;
          if(profopts.profile_online_print_reconfigures) {
            printf("Binding site %d to the lower tier.\n", tree_it_val(sit));
          }
        }

        /* Do the actual rebinding. */
        if(dl) {
          sicm_arena_set_devices(tracker.arenas[tree_it_key(sit)->index]->arena, dl);
        }
      }
    }
    if(profopts.profile_online_print_reconfigures) {
      printf("Reconfigure complete.\n");
      fflush(stdout);
    }

    /* The previous tree can be freed, because we're going to
       overwrite it with the current one */
    tree_free(prev_hotset);
    prof.profile_online.prev_hotset = (void *) hotset;

    /* If applicable, free the merged_sorted_sites tree */
    if(prof.profile_online.last_iter_sorted_sites) {
      tree_traverse(merged_sorted_sites, sit) {
        if(tree_it_key(sit)) {
          orig_free(tree_it_key(sit));
        }
      }
      tree_free(merged_sorted_sites);
    }

    /* Free the sorted_sites tree */
    tree_traverse(sorted_sites, sit) {
      if(tree_it_key(sit)) {
        orig_free(tree_it_key(sit));
      }
    }
    tree_free(sorted_sites);

    prof.profile_online.num_reconfigures++;
  }

  end_interval();
}

void profile_online_init() {
  size_t i;
  char found;
  char *weight;
  char *value;
  char *algo;
  char *sort;

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

  /* Find the event string and index. The event was required to be set at initialization. */
  found = 0;
  for(i = 0; i < prof.profile->num_profile_all_events; i++) {
    if(strcmp(prof.profile->profile_all_events[i], profopts.profile_online_event) == 0) {
      found = 1;
      prof.profile_online.profile_online_event_index = i;
      break;
    }
  }
  if(!found) {
    fprintf(stderr, "Event specified in SH_PROFILE_ONLINE_EVENT is not listed in SH_PROFILE_ALL_EVENTS. Aborting.\n");
    exit(1);
  }

  algo = orig_malloc((strlen("hotset") + 1) * sizeof(char));
  strcpy(algo, "hotset");
  sort = orig_malloc((strlen("value_per_weight") + 1) * sizeof(char));
  strcpy(sort, "value_per_weight");

  /* The previous and current profiling *need* to have the same type of profiling for this
     to make sense. Otherwise, you're just going to get errors. */
  prof.profile_online.last_iter_profile = NULL;
  prof.profile_online.last_iter_sorted_sites = NULL;
  if(profopts.profile_input_file) {
    prof.profile_online.last_iter_profile = sh_parse_profiling(profopts.profile_input_file);
    sh_packing_init(prof.profile_online.last_iter_profile,
                    &value,
                    &profopts.profile_all_events[prof.profile_online.profile_online_event_index],
                    &weight,
                    &algo,
                    &sort,
                    profopts.profile_online_print_reconfigures);
    prof.profile_online.last_iter_sorted_sites = sh_convert_to_site_tree(prof.profile_online.last_iter_profile);
  } else {
    sh_packing_init(prof.profile,
                    &value,
                    &profopts.profile_all_events[prof.profile_online.profile_online_event_index],
                    &weight,
                    &algo,
                    &sort,
                    profopts.profile_online_print_reconfigures);
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
}

void profile_online_deinit() {
}

void profile_online_post_interval(arena_profile *info) {
}

void profile_online_skip_interval(int s) {
  end_interval();
}
