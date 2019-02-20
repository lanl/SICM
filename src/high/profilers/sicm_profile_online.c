#include <sys/stat.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <unistd.h>
#include <sys/syscall.h>
#include <errno.h>
#include <sys/types.h>
#include "sicm_runtime.h"
#include "sicm_profilers.h"
#include "sicm_profile.h"

void profile_online_arena_init(profile_online_info *);
void profile_online_deinit();
void profile_online_init();
void *profile_online(void *);
void profile_online_interval(int);
void profile_online_skip_interval(int);
void profile_online_post_interval(profile_info *);

void profile_online_arena_init(profile_online_info *info) {
}

void *profile_online(void *a) {
  pthread_setcanceltype(PTHREAD_CANCEL_ASYNCHRONOUS, NULL);

  while(1) { }
}

size_t get_value(size_t index, size_t event_index) {
  arena_info *arena;
  profile_info *profinfo;
  per_event_profile_all_info *per_event_profinfo;
  size_t value;

  /* Get the profiling information */
  arena = tracker.arenas[index];
  profinfo = prof.info[index];
  per_event_profinfo = &(profinfo->profile_all.events[event_index]);
  if((!arena) || (!profinfo) || (!profinfo->num_intervals)) {
    return 0;
  }

  return per_event_profinfo->total;
}

/* Gets weight in kilobytes, to match sicm_avail and sicm_capacity. */
size_t get_weight(size_t index) {
  arena_info *arena;
  profile_info *profinfo;
  size_t weight;

  /* Get the profiling information */
  arena = tracker.arenas[index];
  profinfo = prof.info[index];
  if((!arena) || (!profinfo) || (!profinfo->num_intervals)) {
    return 0;
  }
  
  /* TODO: Speed this up by setting something up (perhaps an offset into profinfo)
   * in `profile_online_init`.
   */
  if(profopts.should_profile_allocs) {
    return profinfo->profile_allocs.peak / 1024;
  } else if(profopts.should_profile_extent_size) {
    return profinfo->profile_extent_size.peak/ 1024;
  } else if(profopts.should_profile_rss) {
    return profinfo->profile_rss.peak / 1024;
  }
}

void profile_online_interval(int s) {
  size_t i, upper_avail, lower_avail,
         value, weight,
         event_index;

  /* Sorted sites */
  tree(double, size_t) sorted_arenas;
  tree_it(double, size_t) it;
  double val_per_weight;

  /* Hotset */
  tree(size_t, deviceptr) hotset, coldset;
  tree_it(size_t, deviceptr) hit, tmp_hit;
  size_t hotset_value, hotset_weight,
         coldset_value, coldset_weight;
  char hot, cold_next_site;

  /* Look at how much the application has consumed on each tier */
  upper_avail = sicm_avail(tracker.upper_device);
  lower_avail = sicm_avail(tracker.lower_device);

  event_index = prof.profile_online.profile_online_event_index;

  if(lower_avail < prof.profile_online.lower_avail_initial) {
    /* The lower tier is now being used, so we need to reconfigure. */

    /* Sort arenas by value/weight in the `sorted_arenas` tree */
    sorted_arenas = tree_make(double, size_t); /* val_per_weight -> arena index */
    for(i = 0; i <= tracker.max_index; i++) {
      value = get_value(i, event_index);
      weight = get_weight(i);
      if(!weight) continue;
      /* 0-value sites have a value inversely proportional to their capacity */
      if(!value) value = 1;

      val_per_weight = ((double) value) / ((double) weight);

      /* First see if this value is already in the tree. If so, inch it up just slightly
       * to avoid collisions.
       */
      it = tree_lookup(sorted_arenas, val_per_weight);
      while(tree_it_good(it)) {
        val_per_weight += 0.0000000000000001;
        it = tree_lookup(sorted_arenas, val_per_weight);
      }

      /* Finally insert into the tree */
      tree_insert(sorted_arenas, val_per_weight, i);
    }

    /* Iterate over the sites and greedily pack them into the hotset.
     * Also construct a tree of the sites that didn't make it.
     */
    hotset_value = 0;
    hotset_weight = 0;
    hot = 1;
    cold_next_site = 0;
    hotset = tree_make(size_t, deviceptr);
    coldset = tree_make(size_t, deviceptr);
    it = tree_last(sorted_arenas);
    while(tree_it_good(it)) {
      if(hot) {
        hotset_value += get_value(tree_it_val(it), event_index);
        hotset_weight += get_weight(tree_it_val(it));
        tree_insert(hotset, tree_it_val(it), tracker.upper_device);
      } else {
        coldset_value += get_value(tree_it_val(it), event_index);
        coldset_weight += get_weight(tree_it_val(it));
        tree_insert(coldset, tree_it_val(it), tracker.upper_device);
      }
      if(cold_next_site) {
        hot = 0;
        cold_next_site = 0;
      }
      if(hotset_weight > prof.profile_online.upper_avail_initial) {
        cold_next_site = 1;
      }
      tree_it_prev(it);
    }

    /* If this is the first interval, just make the previous sets
     * empty */
    if(!prof.profile_online.prev_coldset) {
      prof.profile_online.prev_coldset = tree_make(size_t, deviceptr);
    }
    if(!prof.profile_online.prev_hotset) {
      prof.profile_online.prev_hotset = tree_make(size_t, deviceptr);
    }

    if(!profopts.profile_online_nobind) {
      /* Rebind arenas that are newly in the coldset */
      tree_traverse(coldset, hit) {
        tmp_hit = tree_lookup(prof.profile_online.prev_coldset, tree_it_key(hit));
        if(!tree_it_good(tmp_hit)) {
          /* The arena is in the current coldset, but not the previous one.
           * Bind its pages to the lower device.
           */
          printf("Arena %d -> AEP\n", tree_it_key(hit));
          sicm_arena_set_devices(tracker.arenas[tree_it_key(hit)]->arena, /* The arena */
                                 prof.profile_online.lower_dl);           /* The device list */
        }
      }

      /* Rebind arenas that are newly in the hotset */
      tree_traverse(hotset, hit) {
        tmp_hit = tree_lookup(prof.profile_online.prev_hotset, tree_it_key(hit));
        if(!tree_it_good(tmp_hit)) {
          /* The arena is in the current hotset, but not the previous one.
           * Bind its pages to the upper device.
           */
          printf("Arena %d -> DDR\n", tree_it_key(hit));
          sicm_arena_set_devices(tracker.arenas[tree_it_key(hit)]->arena, /* The arena */
                                 prof.profile_online.upper_dl);           /* The device list */
        }
      }
    }

    /* Free the previous trees and set these as the current ones */
    if(prof.profile_online.prev_hotset) {
      tree_free(prof.profile_online.prev_hotset);
    }
    prof.profile_online.prev_hotset = hotset;
    if(prof.profile_online.prev_coldset) {
      tree_free(prof.profile_online.prev_coldset);
    }
    prof.profile_online.prev_coldset = coldset;
  }

  end_interval();
}

void profile_online_init() {
  size_t i;
  char found;

  /* Determine which type of profiling to use to determine weight. Error if none found. */
  /* TODO: Set something up so that we don't have to check this every time `get_weight` is called. */
  if(profopts.should_profile_allocs) {
  } else if(profopts.should_profile_extent_size) {
  } else if(profopts.should_profile_rss) {
  } else {
    fprintf(stderr, "SH_PROFILE_ONLINE requires at least one type of weight profiling. Aborting.\n");
    exit(1);
  }

  /* Look for the event that we're supposed to use for value. Error out if it's not found. */
  if(!profopts.should_profile_all) { 
    fprintf(stderr, "SH_PROFILE_ONLINE requires SH_PROFILE_ALL. Aborting.\n");
    exit(1);
  }
  found = 0;
  for(i = 0; i < profopts.num_profile_all_events; i++) {
    if(strcmp(profopts.profile_all_events[i], profopts.profile_online_event) == 0) {
      found = 1;
      prof.profile_online.profile_online_event_index = i;
      break;
    }
  }
  if(!found) {
    fprintf(stderr, "Event specified in SH_PROFILE_ONLINE_EVENT is not listed in SH_PROFILE_ALL_EVENTS. Aborting.\n");
    exit(1);
  }

  /* Figure out the amount of free memory that we're starting out with */
  prof.profile_online.upper_avail_initial = sicm_avail(tracker.upper_device);
  prof.profile_online.lower_avail_initial = sicm_avail(tracker.lower_device);

  /* Since sicm_arena_set_devices accepts a device_list, construct these */
  prof.profile_online.upper_dl = malloc(sizeof(struct sicm_device_list));
  prof.profile_online.upper_dl->count = 1;
  prof.profile_online.upper_dl->devices = malloc(sizeof(deviceptr));
  prof.profile_online.upper_dl->devices[0] = tracker.upper_device;
  prof.profile_online.lower_dl = malloc(sizeof(struct sicm_device_list));
  prof.profile_online.lower_dl->count = 1;
  prof.profile_online.lower_dl->devices = malloc(sizeof(deviceptr));
  prof.profile_online.lower_dl->devices[0] = tracker.lower_device;

  prof.profile_online.prev_hotset = NULL;
  prof.profile_online.prev_coldset = NULL;
}

void profile_online_deinit() {
}

void profile_online_post_interval(profile_info *info) {
}

void profile_online_skip_interval(int s) {
}
