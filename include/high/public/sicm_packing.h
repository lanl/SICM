#pragma once

/* Packing.
 * Generates a packed hotset/knapsack from SICM's high-level interface's output.
 * First run SICM's high-level interface with one of the profiling methods,
 * then use this program to generate a set of hot allocation sites from that output.
 */

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <inttypes.h>
#include <limits.h>
#include "sicm_tree.h"
#include "sicm_parsing.h"

/* Value flags */
#define PROFILE_ALL_TOTAL 1<<0
#define PROFILE_ALL_CURRENT 1<<1
#define PROFILE_BW_RELATIVE_TOTAL 1<<2
#define PROFILE_BW_RELATIVE_CURRENT 1<<3

/* Weight flags */
#define PROFILE_ALLOCS_PEAK 1<<0
#define PROFILE_ALLOCS_CURRENT 1<<1
#define PROFILE_EXTENT_SIZE_PEAK 1<<2
#define PROFILE_EXTENT_SIZE_CURRENT 1<<3
#define PROFILE_RSS_PEAK 1<<4
#define PROFILE_RSS_CURRENT 1<<5

/* Packing algorithm flags */
#define HOTSET 1<<0
#define THERMOS 1<<1

/* Sort flags */
#define VALUE_PER_WEIGHT 1<<0
#define VALUE 1<<1
#define WEIGHT 1<<2

/* Defaults */
#define DEFAULT_ALGO THERMOS
#define DEFAULT_SORT VALUE_PER_WEIGHT

char *sh_packing_value_str(unsigned int flag) {
  if(flag == PROFILE_ALL_TOTAL) {
    return "profile_all_total";
  } else if(flag == PROFILE_ALL_CURRENT) {
    return "profile_all_current";
  } else if(flag == PROFILE_BW_RELATIVE_TOTAL) {
    return "profile_bw_relative_total";
  } else if(flag == PROFILE_BW_RELATIVE_CURRENT) {
    return "profile_bw_relative_current";
  } else {
    fprintf(stderr, "Invalid value flag. Aborting.\n");
    exit(1);
  }
}

unsigned int sh_packing_value_flag(char *flag) {
  if(strcmp(flag, "profile_all_total") == 0) {
    return PROFILE_ALL_TOTAL;
  } else if(strcmp(flag, "profile_all_current") == 0) {
    return PROFILE_ALL_CURRENT;
  } else if(strcmp(flag, "profile_bw_relative_total") == 0) {
    return PROFILE_BW_RELATIVE_TOTAL;
  } else if(strcmp(flag, "profile_bw_relative_current") == 0) {
    return PROFILE_BW_RELATIVE_CURRENT;
  } else {
    fprintf(stderr, "Invalid value string. Aborting.\n");
    exit(1);
  }
}

char *sh_packing_weight_str(unsigned int flag) {
  if(flag == PROFILE_ALLOCS_PEAK) {
    return "profile_allocs_peak";
  } else if(flag == PROFILE_ALLOCS_CURRENT) {
    return "profile_allocs_current";
  } else if(flag == PROFILE_EXTENT_SIZE_PEAK) {
    return "profile_extent_size_peak";
  } else if(flag == PROFILE_EXTENT_SIZE_CURRENT) {
    return "profile_extent_size_current";
  } else if(flag == PROFILE_RSS_PEAK) {
    return "profile_rss_peak";
  } else if(flag == PROFILE_RSS_CURRENT) {
    return "profile_rss_current";
  } else {
    fprintf(stderr, "Invalid weight flag. Aborting.\n");
    exit(1);
  }
}

unsigned int sh_packing_weight_flag(char *flag) {
  if(strcmp(flag, "profile_allocs_peak") == 0) {
    return PROFILE_ALLOCS_PEAK;
  } else if(strcmp(flag, "profile_allocs_current") == 0) {
    return PROFILE_ALLOCS_CURRENT;
  } else if(strcmp(flag, "profile_extent_size_peak") == 0) {
    return PROFILE_EXTENT_SIZE_PEAK;
  } else if(strcmp(flag, "profile_extent_size_current") == 0) {
    return PROFILE_EXTENT_SIZE_CURRENT;
  } else if(strcmp(flag, "profile_rss_peak") == 0) {
    return PROFILE_RSS_PEAK;
  } else if(strcmp(flag, "profile_rss_current") == 0) {
    return PROFILE_RSS_CURRENT;
  } else {
    fprintf(stderr, "Invalid weight string. Aborting.\n");
    exit(1);
  }
}

char *sh_packing_algo_str(unsigned int flag) {
  if(flag == HOTSET) {
    return "hotset";
  } else if(flag == THERMOS) {
    return "thermos";
  } else {
    fprintf(stderr, "Invalid algo flag. Aborting.\n");
    exit(1);
  }
}

unsigned int sh_packing_algo_flag(char *flag) {
  if(strcmp(flag, "hotset") == 0) {
    return HOTSET;
  } else if(strcmp(flag, "thermos") == 0) {
    return THERMOS;
  } else {
    fprintf(stderr, "Invalid algo string. Aborting.\n");
    exit(1);
  }
}

char *sh_packing_sort_str(unsigned int flag) {
  if(flag == VALUE_PER_WEIGHT) {
    return "value_per_weight";
  } else if(flag == VALUE) {
    return "value";
  } else if(flag == WEIGHT) {
    return "weight";
  } else {
    fprintf(stderr, "Invalid sort flag. Aborting.\n");
    exit(1);
  }
}

unsigned sh_packing_sort_flag(char *flag) {
  if(strcmp(flag, "value_per_weight") == 0) {
    return VALUE_PER_WEIGHT;
  } else if(strcmp(flag, "value") == 0) {
    return VALUE;
  } else if(strcmp(flag, "weight") == 0) {
    return WEIGHT;
  } else {
    fprintf(stderr, "Invalid sort string. Aborting.\n");
    exit(1);
  }
}

/* A struct of options that the user passes to `sh_packing_init` to choose
   how to generate values and weights, which packing algorithm to use, etc. */
typedef struct packing_options {
  unsigned int verbose : 1;
  unsigned int value : 4;
  unsigned int weight : 6;
  unsigned int algo : 2;
  unsigned int sort : 3;
  
  /* Only for PROFILE_ALL_TOTAL and PROFILE_ALL_CURRENT.
     Array of floats to weight each event by. */
  size_t num_profile_all_events,
         num_profile_skts;
  
  FILE *debug_file;
} packing_options;
static packing_options *packopts = NULL;

typedef struct site_profile_info {
  size_t value, weight, num_hot_intervals,
         *value_arr;
  double value_per_weight;
  int index;
  char dev, hot;
} site_profile_info;
typedef site_profile_info * site_info_ptr; /* Required for tree.h */

#ifndef SICM_PACKING /* Make sure we don't define the below trees twice */
#define SICM_PACKING
use_tree(site_info_ptr, int);
use_tree(int, site_info_ptr);
#endif

/* Gets a value for the given arena in aprof, and sets `value` and `value_arr` */
static inline void set_value(arena_profile *aprof,
                             site_info_ptr site) {
  size_t i, tmp;

  site->value = 0;
  if(packopts->value == PROFILE_ALL_TOTAL) {
    site->value_arr = malloc(sizeof(size_t) * packopts->num_profile_all_events);
    for(i = 0; i < packopts->num_profile_all_events; i++) {
      tmp = (aprof->profile_all.events[i].total);
      site->value += tmp;
      site->value_arr[i] = tmp;
    }
  } else if(packopts->value == PROFILE_ALL_CURRENT) {
    site->value_arr = malloc(sizeof(size_t) * packopts->num_profile_all_events);
    for(i = 0; i < packopts->num_profile_all_events; i++) {
      tmp = (aprof->profile_all.events[i].current);
      site->value += tmp;
      site->value_arr[i] = tmp;
    }
  } else if(packopts->value == PROFILE_BW_RELATIVE_TOTAL) {
    site->value_arr = malloc(sizeof(size_t));
    site->value_arr[0] = aprof->profile_bw.total;
    site->value = aprof->profile_bw.total;
  } else if(packopts->value == PROFILE_BW_RELATIVE_CURRENT) {
    site->value_arr = malloc(sizeof(size_t));
    site->value_arr[0] = aprof->profile_bw.current;
    site->value = aprof->profile_bw.current;
  } else {
    fprintf(stderr, "Invalid value type detected. Aborting.\n");
    exit(1);
  }
}

/* Gets a weight from the given arena_profile */
static inline size_t get_weight(arena_profile *aprof) {
  size_t weight;

  if(packopts->weight == PROFILE_ALLOCS_PEAK) {
    weight = aprof->profile_allocs.peak;
  } else if(packopts->weight == PROFILE_ALLOCS_CURRENT) {
    weight = aprof->profile_allocs.current;
  } else if(packopts->weight == PROFILE_EXTENT_SIZE_PEAK) {
    weight = aprof->profile_extent_size.peak;
  } else if(packopts->weight == PROFILE_EXTENT_SIZE_CURRENT) {
    weight = aprof->profile_extent_size.current;
  } else if(packopts->weight == PROFILE_RSS_PEAK) {
    weight = aprof->profile_rss.peak;
  } else if(packopts->weight == PROFILE_RSS_CURRENT) {
    weight = aprof->profile_rss.current;
  } else {
    fprintf(stderr, "Invalid weight type detected. Aborting.\n");
    exit(1);
  }

  return weight;
}

/* This is the function that tree uses to sort the sites.
   This is run each time the tree compares two site_profile_info structs.
   Slow (since it checks these conditions for every comparison), but simple. */
static inline int site_tree_cmp(site_info_ptr a, site_info_ptr b) {
  int retval;

  if(a == b) {
    return 0;
  }

  if(packopts->sort == VALUE_PER_WEIGHT) {
    if(a->value_per_weight < b->value_per_weight) {
      retval = 1;
    } else if(a->value_per_weight > b->value_per_weight) {
      retval = -1;
    } else {
      retval = 1;
    }
  } else if(packopts->sort == VALUE) {
    if(a->value < b->value) {
      retval = 1;
    } else if(a->value > b->value) {
      retval = -1;
    } else {
      retval = 1;
    }
  } else if(packopts->sort == WEIGHT) {
    if(a->weight < b->weight) {
      retval = 1;
    } else if(a->weight > b->weight) {
      retval = -1;
    } else {
      retval = 1;
    }
  } else {
    fprintf(stderr, "Invalid sorting type detected. Aborting.\n");
    exit(1);
  }

  return retval;
}

/* This function merges two site trees (the structure output by sh_convert_to_site_tree).
   It accepts two trees, a float which determines how highly to consider the value of the first tree,
   and another float which determines how highly to consider the weights of the first tree.
   Because this is intended to be used to consider a past run's profiling, only sites that are in
   the second tree are considered at all. */
static tree(site_info_ptr, int) sh_merge_site_trees(tree(site_info_ptr, int) first, tree(site_info_ptr, int) second, float value_ratio, float weight_ratio) {
  tree_it(site_info_ptr, int) sit;
  tree_it(int, site_info_ptr) fit;
  tree(int, site_info_ptr) new_first;
  tree(site_info_ptr, int) merged;
  site_info_ptr site;

  /* Flip the keys and values around for the first tree */
  new_first = tree_make(int, site_info_ptr);
  tree_traverse(first, sit) {
    tree_insert(new_first, tree_it_val(sit), tree_it_key(sit));
  }

  /* Create the merged tree now */
  merged = tree_make_c(site_info_ptr, int, &site_tree_cmp);
  tree_traverse(second, sit) {
    fit = tree_lookup(new_first, tree_it_val(sit));
    site = orig_malloc(sizeof(site_profile_info));
    site->index = tree_it_key(sit)->index;
    if(tree_it_good(fit)) {
      site->value = (tree_it_val(fit)->value * value_ratio) + (tree_it_key(sit)->value * (1 - value_ratio));
      site->weight = (tree_it_val(fit)->weight * weight_ratio) + (tree_it_key(sit)->weight * (1 - weight_ratio));
    } else {
      site->value = tree_it_key(sit)->value;
      site->weight = tree_it_key(sit)->weight;
    }
    site->value_per_weight = ((double) site->value) / site->weight;
    tree_insert(merged, site, tree_it_val(sit));
  }

  /* Since this just stores pointers that were also stored in `first`, we don't need
     to free those up. */
  tree_free(new_first);
  return merged;
}

/* This function iterates over the data structure that stores the profiling information,
   and converts it to a tree of allocation sites and their value and weight amounts.
   If the profiling information includes multiple sites per arena, that arena's profiling
   is simply associated with all of the sites in the arena. This may change in the future. */
static tree(site_info_ptr, int) sh_convert_to_site_tree(application_profile *info, size_t interval) {
  tree(site_info_ptr, int) site_tree;
  tree_it(site_info_ptr, int) sit;
  size_t i, num_arenas;
  int n;
  site_info_ptr site, site_copy;
  arena_profile *aprof;
  interval_profile *cur_interval;

  site_tree = tree_make_c(site_info_ptr, int, &site_tree_cmp);
  
  if(interval == SIZE_MAX) {
    /* If the user didn't specify an interval to use */
    #ifdef SICM_RUNTIME
      /* We're in the runtime library, so we can use the convenience pointer */
      cur_interval = &(info->this_interval);
    #else
      /* I guess just default to the last interval? */
      cur_interval = &(info->intervals[info->num_intervals - 1]);
    #endif
  } else {
    /* The user specified an interval, use that. */
    cur_interval = &(info->intervals[interval]);
  }

  /* Iterate over the arenas, create a site_profile_info struct for each site,
     and simply insert them into the tree (which sorts them). */
  num_arenas = cur_interval->num_arenas;
  for(i = 0; i < num_arenas; i++) {
    aprof = cur_interval->arenas[i];
    if(!aprof) continue;
    if(get_weight(aprof) == 0) continue;

    site = orig_malloc(sizeof(site_profile_info));
    set_value(aprof, site);
    site->weight = get_weight(aprof);
    site->value_per_weight = ((double) site->value) / site->weight;
    site->index = aprof->index;
    site->dev = aprof->profile_online.dev;
    site->hot = aprof->profile_online.hot;
    site->num_hot_intervals = aprof->profile_online.num_hot_intervals;

    for(n = 0; n < aprof->num_alloc_sites; n++) {
      /* We make a copy of this struct for each site to aid freeing this up in the future */
      site_copy = orig_malloc(sizeof(site_profile_info));
      memcpy(site_copy, site, sizeof(site_profile_info));
      tree_insert(site_tree, site_copy, aprof->alloc_sites[n]);
    }
    orig_free(site);
  }

  return site_tree;
}

/* Just flips a site tree (as returned by sh_convert_to_site_tree, etc.) */
static tree(int, site_info_ptr) sh_flip_site_tree(tree(site_info_ptr, int) site_tree) {
  tree(int, site_info_ptr) flipped;
  tree_it(site_info_ptr, int) it;

  flipped = tree_make(int, site_info_ptr);
  tree_traverse(site_tree, it) {
    tree_insert(flipped, tree_it_val(it), tree_it_key(it));
  }

  return flipped;
}

/* Gets the greatest common divisor of all given sites' sizes
 * by just iterating over them and finding the GCD of the current
 * GCD and the current value
 */
static size_t get_gcd(tree(site_info_ptr, int) site_tree) {
  tree_it(site_info_ptr, int) sit;
  size_t gcd, a, b, tmp;

  sit = tree_begin(site_tree);
  gcd = tree_it_key(sit)->weight;
  tree_it_next(sit);
  while(tree_it_good(sit)) {
    /* Find the GCD of a and b */
    a = tree_it_key(sit)->weight;
    b = gcd;
    while(a != 0) {
      tmp = a;
      a = b % a;
      b = tmp;
    }
    gcd = b;

    /* Go on to the next pair of sizes */
    tree_it_next(sit);
  }

  return gcd;
}

static void sh_scale_sites(tree(site_info_ptr, int) site_tree, double scale) {
  tree_it(site_info_ptr, int) sit;
  size_t gcd, multiples, scaled;

  /* The GCD is important for the knapsack packing algorithm to work.
     Instead of scaling each site down and essentially making this
     GCD a value of 1 (instead of 4096 for pages, or 4MB for extents),
     scale sites down by a factor of the GCD.
  */
  gcd = get_gcd(site_tree);

  /* Scale each site */
  tree_traverse(site_tree, sit) {
    /* See how many multiples of the GCD the scaled version is */
    scaled = tree_it_key(sit)->weight * scale;
    if(scaled > gcd) {
      multiples = scaled / gcd;
      tree_it_key(sit)->weight = gcd * multiples;
    } else {
      /* Minimum size of a site is the GCD */
      tree_it_key(sit)->weight = gcd;
    }
  }
}

/* Greedy hotset algorithm. Called by sh_get_hot_sites.
   The resulting tree is keyed on the site ID instead of the site_info_ptr. */
static tree(int, site_info_ptr) get_hotset(tree(site_info_ptr, int) site_tree, size_t capacity) {
  tree(int, site_info_ptr) ret;
  tree_it(site_info_ptr, int) sit;
  char break_next_site;
  size_t packed_size;

  ret = tree_make(int, site_info_ptr);

  /* Iterate over the sites (which have already been sorted), adding them
     greedily, until the capacity is reached. */
  break_next_site = 0;
  packed_size = 0;
  tree_traverse(site_tree, sit) {
    /* If we're over capacity, break. We've already added the site,
     * so we overflow by exactly one site. */
    if(packed_size > capacity) {
      break;
    }
    /* Ignore the site if the value is zero. */
    if((tree_it_key(sit)->value == 0) ||
       (tree_it_key(sit)->weight == 0)) {
      continue;
    }
    if(packopts->verbose) {
      printf("%d: %zu, %zu\n", tree_it_val(sit), tree_it_key(sit)->value, tree_it_key(sit)->weight);
    }
    packed_size += tree_it_key(sit)->weight;
    tree_insert(ret, tree_it_val(sit), tree_it_key(sit));
  }
  
  if(packopts->verbose) {
    printf("%zu / %zu\n", packed_size, capacity);
  }

  return ret;
}

/* Greedy hotset algorithm, but with a twist. Attempts to overpack
   more intelligently than `get_hotset`. Called by sh_get_hot_sites.
   The resulting tree is keyed on the site ID instead of the site_info_ptr. */
static tree(int, site_info_ptr) get_thermos(tree(site_info_ptr, int) site_tree, size_t capacity) {
  tree(int, site_info_ptr) hotset;
  tree_it(site_info_ptr, int) sit;
  tree_it(int, site_info_ptr) hit;
  char break_next_site;
  size_t packed_weight, /* Bytes in the hotset already */
         packed_value, /* Value in the hotset already */
         overpacked_weight,
         would_displace_weight,
         would_displace_value;

  hotset = tree_make(int, site_info_ptr);

  break_next_site = 0;
  packed_weight = 0;
  packed_value = 0;
  tree_traverse(site_tree, sit) {
    
    /* Ignore the site if the value is zero. */
    if((tree_it_key(sit)->value == 0) ||
       (tree_it_key(sit)->weight == 0)) {
      continue;
    }
    
    /* Calculate the amount of bytes that we would overpack by
       if we were to add this site to the hotset */
    if((packed_weight + tree_it_key(sit)->weight) > capacity) {
      overpacked_weight = packed_weight + tree_it_key(sit)->weight - capacity;
    } else {
      overpacked_weight = 0;
    }
    
    if(overpacked_weight) {
      /* The hotset is already above the prescribed limit, but let's add more
         sites if they're hotter than the sites that they would displace */
      would_displace_weight = 0;
      would_displace_value = 0;
      tree_traverse(hotset, hit) {
        /* In this loop, we're just calculating how many bytes and how much
           value we would displace */
        would_displace_weight += tree_it_val(hit)->weight;
        would_displace_value += tree_it_val(hit)->value;
        if(would_displace_weight > overpacked_weight) {
          break;
        }
      }
      if(tree_it_key(sit)->value > would_displace_value) {
        /* If the value of the current site is greater than the sites
           that it would displace, then let's overpack more. */
        packed_weight += tree_it_key(sit)->weight;
        packed_value += tree_it_key(sit)->value;
        tree_insert(hotset, tree_it_val(sit), tree_it_key(sit));
        if(packopts->verbose) {
          printf("(op) %d: %zu, %zu\n", tree_it_val(sit), tree_it_key(sit)->value, tree_it_key(sit)->weight);
          printf("[Overpacking by %zu, %zu > %zu]\n", overpacked_weight, tree_it_key(sit)->value, would_displace_value);
        }
      }
    } else {
      /* We've not reached the capacity yet, so just keep greedily packing. */
      packed_weight += tree_it_key(sit)->weight;
      packed_value += tree_it_key(sit)->value;
      tree_insert(hotset, tree_it_val(sit), tree_it_key(sit));
      if(packopts->verbose) {
        printf("%d: %zu, %zu\n", tree_it_val(sit), tree_it_key(sit)->value, tree_it_key(sit)->weight);
      }
    }
  }
  
  if(packopts->verbose) {
    printf("%zu / %zu\n", packed_weight, capacity);
  }
  
  return hotset;
}

/* Same as `sh_get_hot_sites`, but gives back `num_sites` number of top sites, instead of
   using some maximum capacity. */
static tree(int, site_info_ptr) sh_get_top_sites(tree(site_info_ptr, int) site_tree, uintmax_t num_sites) {
  tree(int, site_info_ptr) ret;
  tree_it(site_info_ptr, int) sit;
  uintmax_t cur_sites;

  ret = tree_make(int, site_info_ptr);

  /* Iterate over the sites (which have already been sorted), adding them
     greedily, until the number of sites has been reached. */
  cur_sites = 0;
  tree_traverse(site_tree, sit) {
    tree_insert(ret, tree_it_val(sit), tree_it_key(sit));
    cur_sites++;
    if(cur_sites == num_sites) {
      break;
    }
  }

  return ret;
}

/* Be careful, this function flips around the keys/values for its return value. */
static tree(int, site_info_ptr) sh_get_hot_sites(tree(site_info_ptr, int) site_tree, uintmax_t capacity) {
  tree(int, site_info_ptr) hot_site_tree;

  if(packopts->algo == HOTSET) {
    hot_site_tree = get_hotset(site_tree, capacity);
  } else if(packopts->algo == THERMOS) {
    hot_site_tree = get_thermos(site_tree, capacity);
  } else {
    fprintf(stderr, "Invalid packing algorithm detected. Aborting.\n");
    exit(1);
  }

  return hot_site_tree;
}


/* Initializes this packing library, sets all of the globals above. Some of the char ** pointers can be pointers to NULL,
   in which case this function will fill them in with a default value. */
static void sh_packing_init(application_profile *info, packing_options **opts) {
  size_t i, n;
  
  packopts = *opts;

  if(!info) {
    fprintf(stderr, "Can't initialize the packing library without some profiling information. Aborting.\n");
    exit(1);
  }
  
  if(!packopts) {
    packopts = orig_calloc(sizeof(char), sizeof(packing_options));
  }

  if(!(packopts->value)) {
    if(info->has_profile_bw_relative) {
      packopts->value = PROFILE_BW_RELATIVE_TOTAL;
    } else if(info->has_profile_all) {
      packopts->value = PROFILE_ALL_TOTAL;
    } else {
      fprintf(stderr, "No value profiling specified, and no value profiling found in the application_profile. Aborting.\n");
      exit(1);
    }
  }
  if(!(packopts->weight)) {
    if(info->has_profile_rss) {
      packopts->weight = PROFILE_RSS_PEAK;
    } else if(info->has_profile_extent_size) {
      packopts->weight = PROFILE_EXTENT_SIZE_PEAK;
    } else if(info->has_profile_allocs) {
      packopts->weight = PROFILE_ALLOCS_PEAK;
    } else {
      fprintf(stderr, "No weight profiling specified, and no weight profiling found in the application_profile. Aborting.\n");
      exit(1);
    }
  }
  if(!(packopts->algo)) {
    packopts->algo = DEFAULT_ALGO;
  }
  if(!(packopts->sort)) {
    packopts->sort = DEFAULT_SORT;
  }
  
  /* Check to make sure the application_profile has the right
     profiling information to fulfill the user's choices. */
  if(((packopts->value == PROFILE_ALL_TOTAL) || (packopts->value == PROFILE_ALL_CURRENT)) &&
     !(info->has_profile_all)) {
    fprintf(stderr, "User chose to use PROFILE_ALL_*, but the application_profile doesn't have that in it.\n");
    exit(1);
  }
  if(((packopts->value == PROFILE_BW_RELATIVE_TOTAL) || (packopts->value == PROFILE_BW_RELATIVE_CURRENT)) &&
     !(info->has_profile_bw_relative)) {
    fprintf(stderr, "User chose to use PROFILE_BW_RELATIVE_*, but the application_profile doesn't have that in it.\n");
    exit(1);
  }
  
  /* Keep track of the number of profile_all and profile_bw events */
  if((packopts->value == PROFILE_ALL_TOTAL) ||
     (packopts->value == PROFILE_ALL_CURRENT)) {
    packopts->num_profile_all_events = info->num_profile_all_events;
  }
  if(packopts->value == PROFILE_BW_RELATIVE_TOTAL) {
    packopts->num_profile_skts = info->num_profile_skts;
  }
  
  /* Print out all relevant options */
  if(packopts->debug_file) {
    fprintf(packopts->debug_file, "===== PACKING OPTIONS =====\n");
    fprintf(packopts->debug_file, "Value: %s\n", sh_packing_value_str(packopts->value));
    fprintf(packopts->debug_file, "Weight: %s\n", sh_packing_weight_str(packopts->weight));
    fprintf(packopts->debug_file, "Algo: %s\n", sh_packing_algo_str(packopts->algo));
    fprintf(packopts->debug_file, "Sort: %s\n", sh_packing_sort_str(packopts->sort));
    fprintf(packopts->debug_file, "===== END PACKING OPTIONS =====\n");
  }
  
  *opts = packopts;
}
