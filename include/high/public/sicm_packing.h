#pragma once

/* Packing.
 * Generates a packed hotset/knapsack from SICM's high-level interface's output.
 * First run SICM's high-level interface with one of the profiling methods,
 * then use this program to generate a set of hot allocation sites from that output. Use this
 * hotset to do a subsequent run.
 */

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <inttypes.h>
#include <limits.h>
#include "sicm_tree.h"
#include "sicm_parsing.h"

/* Default strings */
#define DEFAULT_VALUE "profile_all"
#define DEFAULT_WEIGHT "profile_allocs"
#define DEFAULT_ALGO "hotset"
#define DEFAULT_SORT "value_per_weight"

/* Default values for all options */
static char sh_verbose_flag = 0;  /* 0 for not verbose, 1 for verbose */
static char sh_value_flag = 0;    /* 0 for profile_all */
static char sh_weight_flag = 0;   /* 0 for profile_allocs,
                                     1 for profile_extent_size */
static char sh_algo_flag = 0;     /* 0 for hotset */
static char sh_sort_flag = 0;     /* 0 for `value_per_weight`,
                                     1 for `value`,
                                     2 for `weight` */
static size_t sh_value_event_index = SIZE_MAX; /* Index into the array of events to use for value */

typedef struct site_profile_info {
  size_t value, weight;
  double value_per_weight;
  int index;
} site_profile_info;
typedef site_profile_info * site_info_ptr; /* Required for tree.h */
use_tree(site_info_ptr, int);

/* Gets a value from the given arena_profile */
static size_t get_value(arena_profile *aprof) {
  size_t value;

  if(sh_value_flag == 0) {
    value = aprof->profile_all.events[sh_value_event_index].total;
  } else {
    fprintf(stderr, "Invalid value type detected. Aborting.\n");
    exit(1);
  }

  return value;
}

/* Gets a weight from the given arena_profile */
static size_t get_weight(arena_profile *aprof) {
  size_t weight;

  if(sh_weight_flag == 0) {
    weight = aprof->profile_allocs.peak;
  } else if(sh_weight_flag == 1) {
    weight = aprof->profile_extent_size.peak;
  } else {
    fprintf(stderr, "Invalid weight type detected. Aborting.\n");
    exit(1);
  }

  return weight;
}

/* This is the function that tree uses to sort the sites.
   This is run each time the tree compares two site_profile_info structs.
   Slow (since it checks these conditions for every comparison), but simple. */
static int site_tree_cmp(site_info_ptr a, site_info_ptr b) {
  int retval;

  if(a == b) {
    return 0;
  }

  if(sh_sort_flag == 0) {
    if(a->value_per_weight < b->value_per_weight) {
      retval = 1;
    } else if(a->value_per_weight > b->value_per_weight) {
      retval = -1;
    } else {
      retval = 1;
    }
  } else if(sh_sort_flag == 1) {
    if(a->value < b->value) {
      retval = 1;
    } else if(a->value > b->value) {
      retval = -1;
    } else {
      retval = 1;
    }
  } else if(sh_sort_flag == 2) {
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

/* This function iterates over the data structure that stores the profiling information,
   and converts it to a tree of allocation sites and their value and weight amounts.
   If the profiling information includes multiple sites per arena, that arena's profiling
   is simply associated with all of the sites in the arena. This may change in the future. */
static tree(site_info_ptr, int) sh_convert_to_site_tree(application_profile *info) {
  tree(site_info_ptr, int) site_tree;
  tree_it(site_info_ptr, int) sit;
  size_t i;
  int n;
  site_info_ptr site, site_copy;
  arena_profile *aprof;

  site_tree = tree_make_c(site_info_ptr, int, &site_tree_cmp);

  if(sh_verbose_flag) {
    printf("Generating a tree of sorted sites.\n");
  }

  /* Iterate over the arenas, create a site_profile_info struct for each site,
     and simply insert them into the tree (which sorts them). */
  for(i = 0; i < info->num_arenas; i++) {
    aprof = &(info->arenas[i]);

    if(get_weight(aprof) == 0) {
      continue;
    }

    site = orig_malloc(sizeof(site_profile_info));
    site->value = get_value(aprof);
    site->weight = get_weight(aprof);
    site->value_per_weight = ((double) site->value) / ((double) site->weight);
    site->index = aprof->index;

    for(n = 0; n < aprof->num_alloc_sites; n++) {
      /* We make a copy of this struct for each site to aid freeing this up in the future */
      site_copy = orig_malloc(sizeof(site_profile_info));
      memcpy(site_copy, site, sizeof(site_profile_info));
      tree_insert(site_tree, site_copy, aprof->alloc_sites[n]);
    }
  }

  if(sh_verbose_flag) {
    printf("Sorted sites:\n");
    tree_traverse(site_tree, sit) {
      printf("%d (val: %zu, weight: %zu, v/w: %lf)\n", tree_it_val(sit),
                                                       tree_it_key(sit)->value,
                                                       tree_it_key(sit)->weight,
                                                       tree_it_key(sit)->value_per_weight);
    }
  }

  return site_tree;
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

/* Greedy hotset algorithm. Called by sh_get_hot_sites. */
static tree(site_info_ptr, int) get_hotset(tree(site_info_ptr, int) site_tree, uintmax_t capacity) {
  tree(site_info_ptr, int) ret;
  tree_it(site_info_ptr, int) sit;
  char break_next_site;
  uintmax_t packed_size;

  ret = tree_make(site_info_ptr, int);

  /* Iterate over the sites (which have already been sorted), adding them
     greedily, until the capacity is reached. */
  break_next_site = 0;
  packed_size = 0;
  tree_traverse(site_tree, sit) {
    packed_size += tree_it_key(sit)->weight;
    tree_insert(ret, tree_it_key(sit), tree_it_val(sit));
    if(sh_verbose_flag) {
      printf("Inserting %d (val: %zu, weight: %zu, v/w: %lf)\n", tree_it_val(sit),
                                                                   tree_it_key(sit)->value,
                                                                   tree_it_key(sit)->weight,
                                                                   tree_it_key(sit)->value_per_weight);
    }

    /* If we're over capacity, break. We've already added the site,
     * so we overflow by exactly one site. */
    if(packed_size > capacity) {
      if(sh_verbose_flag) {
        printf("Packed size is %ju, capacity is %ju. That's the last site.\n", packed_size, capacity);
      }
      break;
    }
  }

  return ret;
}

static tree(site_info_ptr, int) sh_get_hot_sites(tree(site_info_ptr, int) site_tree, uintmax_t capacity) {
  tree(site_info_ptr, int) hot_site_tree;

  if(sh_algo_flag == 0) {
    hot_site_tree = get_hotset(site_tree, capacity);
  } else {
    fprintf(stderr, "Invalid packing algorithm detected. Aborting.\n");
    exit(1);
  }

  return hot_site_tree;
}


/* Initializes this packing library, sets all of the globals above. Some of the char ** pointers can be pointers to NULL,
   in which case this function will fill them in with a default value. */
static void sh_packing_init(application_profile *info, char **value, char **event, char **weight, char **algo, char **sort, char verbose) {
  size_t i;

  if(!info) {
    fprintf(stderr, "Can't initialize the packing library without some profiling information. Aborting.\n");
    exit(1);
  }

  /* Set the defaults if any of the dereferenced pointers are NULL */
  if(*value == NULL) {
    *value = orig_malloc((strlen(DEFAULT_VALUE) + 1) * sizeof(char));
    strcpy(*value, DEFAULT_VALUE);
  }
  if(*weight == NULL) {
    *weight = orig_malloc((strlen(DEFAULT_WEIGHT) + 1) * sizeof(char));
    strcpy(*weight, DEFAULT_WEIGHT);
  }
  if(*algo == NULL) {
    *algo = orig_malloc((strlen(DEFAULT_ALGO) + 1) * sizeof(char));
    strcpy(*algo, DEFAULT_ALGO);
  }
  if(*sort == NULL) {
    *sort = orig_malloc((strlen(DEFAULT_SORT) + 1) * sizeof(char));
    strcpy(*sort, DEFAULT_SORT);
  }

  /* Set the sh_value_flag */
  if(strcmp(*value, "profile_all") == 0) {
    sh_value_flag = 0;
  } else {
    fprintf(stderr, "Type of value profiling (%s) not recognized. Aborting.\n", *value);
    exit(1);
  }

  /* Figure out which index the chosen event is */
  if(sh_value_flag == 0) {
    if(*event == NULL) {
      /* Just grab the first event in the value's list of events */
      if(info->num_profile_all_events) {
        *event = orig_malloc(strlen(info->profile_all_events[0]) * sizeof(char));
        strcpy(*event, info->profile_all_events[0]);
      } else {
        fprintf(stderr, "The chosen value profiling has no events to default to. Aborting.\n");
        exit(1);
      }
    } else {
      /* The user specified an event, so try to find that specific one */
      for(i = 0; i < info->num_profile_all_events; i++) {
        if(strcmp(*event, info->profile_all_events[i]) == 0) {
          sh_value_event_index = i;
        }
      }
      if(sh_value_event_index == SIZE_MAX) {
        fprintf(stderr, "Unable to find the event '%s' in the profiling. Aborting.\n", event);
        exit(1);
      }
    }
  } else {
    fprintf(stderr, "Invalid value profiling detected. Aborting.\n");
    exit(1);
  }

  /* Set sh_weight_flag */
  if(strcmp(*weight, "profile_allocs") == 0) {
    sh_weight_flag = 0;
  } else if(strcmp(*weight, "profile_extent_size") == 0) {
    sh_weight_flag = 1;
  } else {
    fprintf(stderr, "Type of weight profiling not recognized. Aborting.\n");
    exit(1);
  }

  /* Set sh_algo_flag */
  if(strcmp(*algo, "hotset") == 0) {
    sh_algo_flag = 0;
  } else {
    fprintf(stderr, "Type of packing algorithm not recognized. Aborting.\n");
    exit(1);
  }

  /* Set sh_sort_flag */
  if(strcmp(*sort, "value_per_weight") == 0) {
    sh_sort_flag = 0;
  } else if(strcmp(*sort, "value") == 0) {
    sh_sort_flag = 1;
  } else if(strcmp(*sort, "weight") == 0) {
    sh_sort_flag = 2;
  } else {
    fprintf(stderr, "Type of sorting not recognized. Aborting.\n");
    exit(1);
  }

  /* Set sh_verbose_flag */
  sh_verbose_flag = verbose;

  /* Print out all values that we initialized, for debugging */
  if(sh_verbose_flag) {
    printf("Finished initializing the packing library with the following parameters:\n");
    printf("  Value: %s\n", *value);
    printf("  Event: %s\n", *event);
    printf("  Weight: %s\n", *weight);
    printf("  Algorithm: %s\n", *algo);
    printf("  Sorting Type: %s\n", *sort);
  }
}
