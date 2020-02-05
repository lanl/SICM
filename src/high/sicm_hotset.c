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
#include "sicm_packing.h"

static struct option long_options[] = {
  {"verbose", no_argument, NULL, 'v'},
  {"value", required_argument, NULL, 'l'},  /* The type of profiling to use
                                               for determining the "value" of an arena.
                                               Defaults to `profile_all`. */
  {"event", required_argument, NULL, 'e'},  /* The event to use to determine the value of an arena.
                                               Defaults to `MEM_LOAD_UOPS_RETIRED:L3_MISS`. */
  {"weight", required_argument, NULL, 'w'}, /* The type of profiling to use as the "weight" of an arena.
                                               Defaults to `profile_extent_size`. */
  {"algo", required_argument, NULL, 'a'},   /* The packing algorithm. Defaults to `hotset`. */
  {"capacity", required_argument, NULL, 'c'},    /* The capacity to pack into. */
  {"node", required_argument, NULL, 'n'},   /* The node that the chosen arenas should go onto. */
  {"scale", required_argument, NULL, 's'},  /* What factor should we scale the weight down by? */
  {"sort", required_argument, NULL, 'o'},   /* How should we sort the sites? Possibilities are
                                               `value_per_weight`, `value`, and `weight`. Defaults
                                               to `value_per_weight`. */
  {"multiplier", required_argument, NULL, 'm'}, /* The multiplier to multiply an event by */
  {0, 0, 0, 0}
};

/* Reads in profiling information from stdin, then runs the packing algorithm
 * based on arguments. Prints the hotset to stdout.
 */
int main(int argc, char **argv) {
  int option_index;
  char c,
       *endptr;
  size_t i;

  char verbose = 0;
  long int node = -1;
  uintmax_t capacity = 0;
  char *value = NULL;
  char *weight = NULL;
  char *algo = NULL;
  char *sort = NULL;
  char **events = NULL;
  float *multipliers = NULL;
  double scale = 0.0;
  size_t num_events = 0;
  size_t num_multipliers = 0;

  /* The profiling information, as parsed by sicm_parsing.h. */
  application_profile *info;
  tree(site_info_ptr, int) site_tree;

  /* The set of hot sites that we've chosen */
  tree(int, site_info_ptr) hot_site_tree;
  tree_it(int, site_info_ptr) sit;

  /* Use getopt to read in the options. */
  option_index = 0;
  while(1) {
    c = getopt_long(argc, argv, "vl:e:w:a:c:n:s:o:m:", long_options, &option_index);
    if(c == -1) {
      break;
    }

    switch(c) {
      case 0:
        /* This is an option that just sets a flag. Ignore it. */
        break;
      case 'v':
        verbose = 1;
        break;
      case 'l':
        /* value */
        value = malloc((strlen(optarg) + 1) * sizeof(char));
        strcpy(value, optarg);
        break;
      case 'e':
        /* event */
        num_events++;
        events = realloc(events, num_events * sizeof(char *));
        events[num_events - 1] = malloc((strlen(optarg) + 1) * sizeof(char));
        strcpy(events[num_events - 1], optarg);
        break;
      case 'w':
        /* weight */
        weight = malloc((strlen(optarg) + 1) * sizeof(char));
        strcpy(weight, optarg);
        break;
      case 'a':
        /* algo */
        algo = malloc((strlen(optarg) + 1) * sizeof(char));
        strcpy(algo, optarg);
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
        sort = malloc((strlen(optarg) + 1) * sizeof(char));
        strcpy(sort, optarg);
        break;
      case 'm':
        /* multiplier */
        num_multipliers++;
        multipliers = realloc(multipliers, num_multipliers * sizeof(float));
        endptr = NULL;
        multipliers[num_multipliers - 1] = strtod(optarg, &endptr);
        break;
      case '?':
        /* We're relying on getopt_long to print an error message. */
        break;
      default:
        fprintf(stderr, "Didn't understand the return value of getopt_long. Debug this, please.\n");
        exit(1);
    }
  }

  /* These are the only two arguments that we can't set a default for */
  if(capacity == 0) {
    fprintf(stderr, "You didn't specify a capacity to pack into. This is required. Aborting.\n");
    exit(1);
  }
  if(node == -1) {
    fprintf(stderr, "You didn't specify a node to pack into. This is required. Aborting.\n");
    exit(1);
  }
  if(num_events == 0) {
    /* Here, the user didn't specify any events. Just set this to one, and let the `events` array be NULL */
    events = malloc(sizeof(char *) * 1);
    events[0] = NULL;
    num_events = 1;
  }
  if((num_multipliers != 0) && (num_multipliers != num_events)) {
    /* The user specified multipliers, but didn't specify the right amount. */
    fprintf(stderr, "You specified multipliers, but didn't specify the same amount as you did events. Aborting.\n");
    exit(1);
  }

  /* Parse profiling information */
  info = sh_parse_profiling(stdin);

  /* Initialize the global options */
  sh_packing_init(info, &value, &events, &num_events, &weight, &algo, &sort, multipliers, verbose);

  /* For the sake of simplicity, convert the parsed profiling information into simpler trees */
  site_tree = sh_convert_to_site_tree(info, info->num_intervals - 1);

  /* Scale the weight of each site down by this factor */
  if(scale) {
    sh_scale_sites(site_tree, scale);
  }

  hot_site_tree = sh_get_hot_sites(site_tree, capacity);

  /* Print out the guidance file */
  printf("===== GUIDANCE =====\n");
  tree_traverse(hot_site_tree, sit) {
    printf("%d %ld\n", tree_it_key(sit), node);
  }
  printf("===== END GUIDANCE =====\n");

  /* Print debugging information about how we generated this guidance file
     Here, we'll assume all of these values are valid, so no errors. */
  printf("Value profiling type: %s\n", value);
  for(i = 0; i < num_events; i++) {
    printf("Value event: %s\n", events[i]);
  }
  printf("Weight profiling type: %s\n", weight);
  printf("Capacity that we packed into: %ju\n", capacity);
  printf("NUMA node: %ld\n", node);
  printf("Scale factor: %lf\n", scale);
  printf("Packing algorithm: %s\n", algo);
  printf("Sorting type: %s\n", sort);
}
