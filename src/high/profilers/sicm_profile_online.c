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
  tree(site_info_ptr, int) hotset;
  tree_it(site_info_ptr, int) sit, old, new;

  /* Look at how much the application has consumed on each tier */
  upper_avail = sicm_avail(tracker.upper_device);
  lower_avail = sicm_avail(tracker.lower_device);

  if(lower_avail < prof.profile_online.lower_avail_initial) {
    /* The lower tier is now being used, so we need to reconfigure. */

    /* Convert to a tree of sites and generate the new hotset */
    sorted_sites = sh_convert_to_site_tree(prof.profile);
    hotset = sh_get_hot_sites(sorted_sites, prof.profile_online.upper_avail_initial);

    /* If this is the first interval, the previous hotset was the empty set */
    if(!prof.profile_online.prev_hotset) {
      prof.profile_online.prev_hotset = tree_make(site_info_ptr, int);
    }

    if(!profopts.profile_online_nobind) {
      /* Iterate over all of the sites. Rebind if:
         1. A site wasn't in the previous hotset, but is in the current one.
         2. A site was in the previous hotset, but now isn't. */
      tree_traverse(sorted_sites, sit) {
        /* Look to see if it's in the new or old hotsets.
           Here, the keys are actually pointers, but they're still
           unique identifiers, so that's all right. */
        old = tree_lookup(prof.profile_online.prev_hotset, tree_it_key(sit));
        new = tree_lookup(hotset, tree_it_key(sit));
        if(tree_it_good(new) && !tree_it_good(old)) {
          dl = prof.profile_online.upper_dl;
        } else if(!tree_it_good(new) && tree_it_good(old)) {
          dl = prof.profile_online.lower_dl;
        }

        /* Do the actual rebinding. */
        sicm_arena_set_devices(tree_it_key(sit)->index, dl);
      }
    }

    /* The previous tree can be freed, because we're going to
       overwrite it with the current one */
    if(prof.profile_online.prev_hotset) {
      tree_free(prof.profile_online.prev_hotset);
    }
    prof.profile_online.prev_hotset = hotset;

    /* Free the sorted_arenas tree */
    tree_traverse(sorted_sites, sit) {
      if(tree_it_key(sit)) {
        orig_free(tree_it_key(sit));
      }
    }
    tree_free(sorted_sites);
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

  if(profopts.profile_file) {
    sh_packing_init(prof.prev_profile,
                    &value,
                    &profopts.profile_all_events[prof.profile_online.profile_online_event_index],
                    &weight,
                    &algo,
                    &sort,
                    1);
  }

  /* Figure out the amount of free memory that we're starting out with */
  prof.profile_online.upper_avail_initial = sicm_avail(tracker.upper_device);
  prof.profile_online.lower_avail_initial = sicm_avail(tracker.lower_device);

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
  prof.profile_online.prev_coldset = NULL;
}

void profile_online_deinit() {
}

void profile_online_post_interval(arena_profile *info) {
}

void profile_online_skip_interval(int s) {
  end_interval();
}
