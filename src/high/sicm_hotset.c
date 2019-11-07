/* Hotset
 * Generates a hotset from SICM's high-level interface's output.
 * First run SICM's high-level interface with one of the profiling methods,
 * then use this program to generate a hotset from that output. Use this
 * hotset to do a subsequent run.
 */

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <inttypes.h>
#include <limits.h>
#include <getopt.h>
#include "sicm_tree.h"
#include "sicm_parsing.h"

/* Default values for all options */
static uintmax_t capacity = 0; /* Capacity in bytes that we should pack into. */
static long int node = 1;      /* NUMA node to associate the "hot" sites with */
static double scale = 0;       /* Factor by which to scale weights by */
static int verbose_flag = 0;   /* 0 for not verbose, 1 for verbose */
static char value_flag = 0;    /* 0 for profile_all */
static char weight_flag = 0;   /* 0 for profile_allocs */
static char algo_flag = 0;     /* 0 for hotset */
static char sort_flag = 0;     /* 0 for `value_per_weight`,
                                  1 for `value`,
                                  2 for `weight` */
static char *event = NULL; /* Event string to use for value. Default set in main(). */
static size_t value_event_index = SIZE_MAX; /* Index into the array of events to use for value */

static struct option long_options[] = {
  {"verbose", no_argument, &verbose_flag, 1},
  {"value", required_argument, NULL, 'v'},  /* The type of profiling to use
                                               for determining the "value" of an arena.
                                               Defaults to `profile_all`. */
  {"event", required_argument, NULL, 'e'},  /* The event to use to determine the value of an arena.
                                               Defaults to `MEM_LOAD_UOPS_RETIRED:L3_MISS`. */
  {"weight", required_argument, NULL, 'w'}, /* The type of profiling to use as the "weight" of an arena.
                                               Defaults to `profile_allocs`. */
  {"algo", required_argument, NULL, 'a'},   /* The packing algorithm. Defaults to `hotset`. */
  {"cap", required_argument, NULL, 'c'},    /* The capacity to pack into. */
  {"node", required_argument, NULL, 'n'},   /* The node that the chosen arenas should go onto. */
  {"scale", required_argument, NULL, 's'},  /* What factor should we scale the weight down by? */
  {"sort", required_argument, NULL, 'o'},   /* How should we sort the sites? Possibilities are
                                               `value_per_weight`, `value`, and `weight`. Defaults
                                               to `value_per_weight`. */
  {0, 0, 0, 0}
};

typedef struct site_profile_info {
  size_t value, weight;
  double value_per_weight;
} site_profile_info;
typedef site_profile_info * site_info_ptr; /* Required for tree.h */
use_tree(site_info_ptr, int);

/* Gets a value from the given prev_profile_info */
size_t get_value(prev_profile_info *arena_info) {
  size_t value;

  if(value_flag == 0) {
    value = arena_info->info.profile_all.events[value_event_index].total;
  } else {
    fprintf(stderr, "Invalid value type detected. Aborting.\n");
    exit(1);
  }

  return value;
}

/* Gets a weight from the given prev_profile_info */
size_t get_weight(prev_profile_info *arena_info) {
  size_t weight;

  if(weight_flag == 0) {
    weight = arena_info->info.profile_allocs.peak;
  } else {
    fprintf(stderr, "Invalid weight type detected. Aborting.\n");
    exit(1);
  }

  return weight;
}

/* This is the function that tree uses to sort the sites.
   This is run each time the tree compares two site_profile_info structs.
   Slow (since it checks these conditions for every comparison), but simple. */
int site_tree_cmp(site_info_ptr a, site_info_ptr b) {
  int retval;

  if(sort_flag == 0) {
    if(a->value_per_weight < b->value_per_weight) {
      retval = 1;
    } else if(a->value_per_weight > b->value_per_weight) {
      retval = -1;
    } else {
      retval = 1;
    }
  } else if(sort_flag == 1) {
    if(a->value < b->value) {
      retval = 1;
    } else if(a->value > b->value) {
      retval = -1;
    } else {
      retval = 1;
    }
  } else if(sort_flag == 2) {
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
tree(site_info_ptr, int) convert_to_site_tree(prev_app_info *info) {
  tree(site_info_ptr, int) site_tree;
  tree_it(site_info_ptr, int) sit;
  size_t i;
  int n;
  site_info_ptr site, site_copy;
  prev_profile_info *arena_info;

  site_tree = tree_make_c(site_info_ptr, int, &site_tree_cmp);

  /* Iterate over the arenas, create a site_profile_info struct for each site,
     and simply insert them into the tree (which sorts them). */
  for(i = 0; i < info->num_arenas; i++) {
    arena_info = &(info->prev_info_arr[i]);

    site = malloc(sizeof(site_info));
    site->value = get_value(arena_info);
    site->weight = get_weight(arena_info);
    site->value_per_weight = ((double) site->value) / ((double) site->weight);

    for(n = 0; n < arena_info->num_alloc_sites; n++) {
      /* We make a copy of this struct for each site to aid freeing this up in the future */
      site_copy = malloc(sizeof(site_info));
      memcpy(site_copy, site, sizeof(site_info));
      tree_insert(site_tree, site_copy, arena_info->alloc_sites[n]);
    }
  }

  if(verbose_flag) {
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
size_t get_gcd(tree(site_info_ptr, int) site_tree) {
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

void scale_sites(tree(site_info_ptr, int) site_tree, double scale) {
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

/* Greedy hotset algorithm. */
tree(site_info_ptr, int) get_hotset(tree(site_info_ptr, int) site_tree) {
  tree(site_info_ptr, int) ret;
  tree_it(site_info_ptr, int) sit;
  char break_next_site;
  size_t packed_size;

  ret = tree_make(site_info_ptr, int);

  /* Iterate over the sites (which have already been sorted), adding them
     greedily, until the capacity is reached. */
  break_next_site = 0;
  packed_size = 0;
  tree_traverse(site_tree, sit) {
    packed_size += tree_it_key(sit)->weight;
    tree_insert(ret, tree_it_key(sit), tree_it_val(sit));

    /* If we're over capacity, break. We've already added the site,
     * so we overflow by exactly one site. */
    if(packed_size > capacity) {
      break;
    }
  }

  return ret;
}

/* Reads in profiling information from stdin, then runs the packing algorithm
 * based on arguments. Prints the hotset to stdout.
 */
int main(int argc, char **argv) {
  int option_index;
  char c,
       *endptr;
  size_t i;

  /* The profiling information, as parsed by sicm_parsing.h. */
  prev_app_info *info;
  tree(site_info_ptr, int) site_tree;

  /* The set of hot sites that we've chosen */
  tree(site_info_ptr, int) hot_site_tree;
  tree_it(site_info_ptr, int) sit;

  /* Set a default event */
  event = malloc(30 * sizeof(char));
  strcpy(event, "MEM_LOAD_UOPS_RETIRED:L3_MISS");

  /* Use getopt to read in the options. */
  option_index = 0;
  while(1) {
    c = getopt_long(argc, argv, "v:e:w:a:c:n:s:", long_options, &option_index);
    if(c == -1) {
      break;
    }

    switch(c) {
      case '0':
        /* This is an option that just sets a flag. Ignore it. */
        break;
      case 'v':
        /* value */
        if(strcmp("profile_all", optarg) == 0) {
          value_flag = 0;
        } else {
          fprintf(stderr, "Type of value profiling not recognized. Aborting.\n");
          exit(1);
        }
        break;
      case 'e':
        /* event */
        /* Here, we'll just store the string of the event. We can figure out
           its index into the array after we've parsed the profiling information. */
        free(event);
        event = malloc((strlen(optarg) + 1) * sizeof(char));
        strcpy(event, optarg);
      case 'w':
        /* weight */
        if(strcmp("profile_allocs", optarg) == 0) {
        } else {
          fprintf(stderr, "Type of weight profiling not recognized. Aborting.\n");
          exit(1);
        }
        break;
      case 'a':
        /* algo */
        if(strcmp("hotset", optarg) == 0) {
          algo_flag = 0;
        } else {
          fprintf(stderr, "Packing algorithm not recognized. Aborting.\n");
          exit(1);
        }
        break;
      case 'c':
        /* capacity */
        endptr = NULL;
        capacity = strtoumax(optarg, &endptr, 10);
        break;
      case 'n':
        /* node */
        /* This is just an integer that we print out into the guidance file.
           Since this program could potentially be used to generate guidance
           for another machine, don't check if this is even a valid NUMA node. */
        endptr = NULL;
        node = strtol(optarg, &endptr, 10);
        break;
      case 's':
        /* scale */
        endptr = NULL;
        scale = strtod(optarg, &endptr);
        break;
      case 'o':
        /* sort */
        if(strcmp("value_per_weight", optarg) == 0) {
          sort_flag = 0;
        } else if(strcmp("value", optarg) == 0) {
          sort_flag = 1;
        } else if(strcmp("weight", optarg) == 0) {
          sort_flag = 2;
        } else {
          fprintf(stderr, "Sorting type not recognized. Aborting.\n");
          exit(1);
        }
      case '?':
        /* We're relying on getopt_long to print an error message. */
        break;
      default:
        fprintf(stderr, "Didn't understand the return value of getopt_long. Debug this, please.\n");
        exit(1);
    }
  }

  if(capacity == 0) {
    fprintf(stderr, "You didn't specify a capacity to pack into. This is required. Aborting.\n");
    exit(1);
  }

  /* Parse profiling information */
  info = sh_parse_profiling(stdin);

  /* Figure out which index the chosen event is */
  if(value_flag == 0) {
    for(i = 0; i < info->num_profile_all_events; i++) {
      if(strcmp(event, info->profile_all_events[i]) == 0) {
        value_event_index = i;
      }
    }
  } else {
    fprintf(stderr, "Invalid value profiling detected. Aborting.\n");
    exit(1);
  }
  if(value_event_index == SIZE_MAX) {
    fprintf(stderr, "Unable to find the event '%s' in the profiling. Aborting.\n", event);
    exit(1);
  }

  /* For the sake of simplicity, convert the parsed profiling information into simpler trees */
  site_tree = convert_to_site_tree(info);

  /* Scale the weight of each site down by this factor */
  if(scale) {
    scale_sites(site_tree, scale);
  }

  if(algo_flag == 0) {
    hot_site_tree = get_hotset(site_tree);
  } else {
    fprintf(stderr, "Packing algorithm not yet implemented. Aborting.\n");
    exit(1);
  }

  /* Print out the guidance file */
  printf("===== GUIDANCE =====\n");
  tree_traverse(hot_site_tree, sit) {
    printf("%d %ld\n", tree_it_val(sit), (int) node);
  }
  printf("===== END GUIDANCE =====\n");

  /* Print debugging information about how we generated this guidance file
     Here, we'll assume all of these values are valid, so no errors. */
  printf("Value profiling type: ");
  if(value_flag == 0) {
    printf("profile_all\n");
  }
  printf("Value event: %s\n", event);
  printf("Value event index: %zu\n", value_event_index);
  printf("Weight profiling type: ");
  if(weight_flag == 0) {
    printf("profile_allocs\n");
  }
  printf("Capacity that we packed into: %ju\n", capacity);
  printf("NUMA node: %ld\n", node);
  printf("Scale factor: %lf\n", scale);
  printf("Packing algorithm: ");
  if(algo_flag == 0) {
    printf("hotset\n");
  }
  printf("Sorting type: ");
  if(sort_flag == 0) {
    printf("value_per_weight\n");
  } else if(sort_flag == 1) {
    printf("value\n");
  } else if(sort_flag == 2) {
    printf("weight\n");
  }
}
