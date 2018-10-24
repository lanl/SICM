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
#include "tree.h"

typedef struct site {
  float bandwidth;
  size_t peak_rss, accesses;
} site;
typedef site * siteptr;

use_tree(unsigned, siteptr);
use_tree(siteptr, unsigned);

/* A comparison function to compare two site structs.  Used to created the
 * sorted_sites tree. Uses bandwidth/byte as the metric for comparison, and
 * size of the site if those are exactly equal.
 */
int bandwidth_cmp(siteptr a, siteptr b) {
  float a_bpb, b_bpb;
  int retval;

  if(a == b) return 0;

  a_bpb = a->bandwidth / a->peak_rss;
  b_bpb = b->bandwidth / b->peak_rss;

  /* Maximize bandwidth/byte */
  if(a_bpb < b_bpb) {
    retval = 1;
  } else if(a_bpb > b_bpb) {
    retval = -1;
  } else {
    /* If identical bandwidth/byte, get smaller of the two */
    if(a->peak_rss < b->peak_rss) {
      retval = 1;
    } else if(a->peak_rss > b->peak_rss) {
      retval = -1;
    } else {
      retval = -1;
    }
  }
  return retval;
}

/* A comparison function to compare two site structs.  Used to created the
 * sorted_sites tree. Uses accesses/byte as the metric for comparison, and
 * size of the site if those are exactly equal.
 */
int accesses_cmp(siteptr a, siteptr b) {
  float a_bpb, b_bpb;
  int retval;

  if(a == b) return 0;

  a_bpb = a->accesses / a->peak_rss;
  b_bpb = b->accesses / b->peak_rss;

  /* Maximize bandwidth/byte */
  if(a_bpb < b_bpb) {
    retval = 1;
  } else if(a_bpb > b_bpb) {
    retval = -1;
  } else {
    /* If identical bandwidth/byte, get smaller of the two */
    if(a->peak_rss < b->peak_rss) {
      retval = 1;
    } else if(a->peak_rss > b->peak_rss) {
      retval = -1;
    } else {
      retval = -1;
    }
  }
  return retval;
}

/* Gets the peak RSS of the whole application by summing the peak_rss of each
 * site
 */
size_t get_peak_rss(tree(unsigned, siteptr) sites) {
  tree_it(unsigned, siteptr) it;
  size_t total;

  total = 0;
  tree_traverse(sites, it) {
    total += tree_it_val(it)->peak_rss;
  }

  return total;
}

/* Gets the greatest common divisor of all given sites' sizes
 * by just iterating over them and finding the GCD of the current
 * GCD and the current value
 */
size_t get_gcd(tree(unsigned, siteptr) sites) {
  tree_it(unsigned, siteptr) it;
  size_t gcd, a, b, tmp;

  it = tree_begin(sites);
  gcd = tree_it_val(it)->peak_rss;
  tree_it_next(it);
  while(tree_it_good(it)) {
    /* Find the GCD of a and b */
    a = tree_it_val(it)->peak_rss;
    b = gcd;
    while(a != 0) {
      tmp = a;
      a = b % a;
      b = tmp;
    }
    gcd = b;

    /* Go on to the next pair of sizes */
    tree_it_next(it);
  }

  return gcd;
}

/* Input is a tree of sites and the capacity (in bytes) that you want to fill
 * up to. Outputs the optimal knapsack. Uses dynamic programming combined with
 * an approximation optimization to limit the amount of memory and runtime it
 * uses.
 */
tree(unsigned, siteptr) get_knapsack(tree(unsigned, siteptr) sites, size_t capacity, char proftype) {
  size_t gcd, num_sites, num_weights, i, j, weight;
  float **table, a, b;
  tree_it(unsigned, siteptr) it;

  /* Create a matrix with 'i' rows and 'j' columns, where 'i' is the number of
   * sites and 'j' is the number of weights (multiples of the gcd, though). We
   * can find 'j' by dividing the peak RSS of the whole application by the GCD.
   */
  gcd = get_gcd(sites);
  num_sites = tree_len(sites);
  num_weights = (capacity / gcd) + 1;
  table = malloc(sizeof(float *) * (num_sites + 1));
  for(i = 0; i <= num_sites; i++) {
    table[i] = calloc(num_weights, sizeof(float));
  }

  /* Build the table by going over all except the first site
   * and calculating the value of each weight limit with that
   * site included.
   */
  it = tree_begin(sites);
  i = 1;
  while(tree_it_good(it)) {
    for(j = 1; j < num_weights; j++) {
      weight = tree_it_val(it)->peak_rss / gcd;
      if(weight > j) {
        table[i][j] = table[i - 1][j];
      } else {
        a = table[i - 1][j];
        if(proftype == 0) {
          b = table[i - 1][j - weight] + tree_it_val(it)->bandwidth;
        } else {
          b = table[i - 1][j - weight] + tree_it_val(it)->accesses;
        }
        if(a > b) {
          table[i][j] = a;
        } else {
          table[i][j] = b;
        }
      }
    }
    tree_it_next(it);
    i++;
  }

  /* At this point, the maximum value can be found in
   * table[num_sites][num_weights - 1]. */

  /* Now figure out which sites that included by walking
   * the table backwards and accumulating the sites in the
   * output structure */
  it = tree_last(sites);
  i = num_sites;
  j = num_weights - 1;
  weight = 0;
  printf("Final knapsack:\n");
  while(j > 0) {
    if(!tree_it_good(it) || (i == 0)) {
      break;
    }
    if(table[i][j] != table[i - 1][j]) {
      printf("%u ", tree_it_key(it));
      weight += tree_it_val(it)->peak_rss;
      j = j - (tree_it_val(it)->peak_rss / gcd);
    }
    i--;
    tree_it_prev(it);
  }
  printf("\n");
  printf("Used capacity: %zu bytes\n", weight);
  printf("Total value: %f\n", table[num_sites][num_weights - 1]);

  for(i = 0; i <= num_sites; i++) {
    free(table[i]);
  }
  free(table);
}

/* Returns a filled hotset given a tree of sites */
tree(unsigned, siteptr) get_hotset(tree(unsigned, siteptr) sites) {
}

/* Returns a filled Thermos hotset given a tree of sites */
tree(unsigned, siteptr) get_thermos(tree(unsigned, siteptr) sites) {
}

/* Reads in profiling information from stdin, then runs the packing algorithm
 * based on arguments. Prints the hotset to stdout.
 */
int main(int argc, char **argv) {
  char *line, *tok, *endptr, proftype, algo, captype;
  ssize_t len, read;
  size_t cap_bytes;
  long long num_sites;
  siteptr cur_site;
  int mbi, pebs, pebs_site, i;
  float bandwidth, cap_float;
  tree(unsigned, siteptr) sites;
  tree_it(unsigned, siteptr) it;

  /* Read in the arguments */
  if(argc != 5) {
    fprintf(stderr, "USAGE: ./hotset proftype algo captype cap\n");
    fprintf(stderr, "proftype: mbi or pebs, the type of profiling.\n");
    fprintf(stderr, "algo: knapsack, hotset, or thermos. The packing algorithm.\n");
    fprintf(stderr, "captype: ratio or constant. The type of capacity.\n");
    fprintf(stderr, "cap: The capacity. A float 0-1 if captype is 'ratio', or a\n");
    fprintf(stderr, "  constant number of bytes otherwise.\n");
    exit(1);
  }
  if(strcmp(argv[1], "mbi") == 0) {
    proftype = 0;
  } else if(strcmp(argv[1], "pebs") == 0) {
    proftype = 1;
  } else {
    fprintf(stderr, "Proftype not recognized. Aborting.\n");
    exit(1);
  }
  if(strcmp(argv[2], "knapsack") == 0) {
    algo = 0;
  } else if(strcmp(argv[2], "hotset") == 0) {
    algo = 1;
  } else if(strcmp(argv[2], "thermos") == 0) {
    algo = 2;
  } else {
    fprintf(stderr, "Algo not recognized. Aborting.\n");
    exit(1);
  }
  if(strcmp(argv[3], "ratio") == 0) {
    captype = 0;
    endptr = NULL;
    cap_float = strtof(argv[4], &endptr);
  } else if(strcmp(argv[3], "constant") == 0) {
    captype = 1;
    cap_bytes = strtoimax(argv[4], &endptr, 10);
  } else {
    fprintf(stderr, "Captype not recognized. Aborting.\n");
    exit(1);
  }

  sites = tree_make(unsigned, siteptr);

  /* Read in from stdin and fill in the tree with the sites */
  num_sites = 0;
  mbi = 0;
  pebs = 0;
  pebs_site = 0;
  line = NULL;
  len = 0;
  while(read = getline(&line, &len, stdin) != -1) {

    /* Find the beginning or end of some results */
    tok = strtok(line, " ");
    if(!tok) break;
    if(strcmp(tok, "=====") == 0) {
      /* Get whether it's the end of results, or MBI, or PEBS */
      tok = strtok(NULL, " ");
      if(!tok) break;
      if(strcmp(tok, "MBI") == 0) {
        /* Need to keep parsing to get the site number */
        tok = strtok(NULL, " ");
        if(!tok || strcmp(tok, "RESULTS") != 0) {
          fprintf(stderr, "Error parsing.\n");
          exit(1);
        }
        tok = strtok(NULL, " ");
        if(!tok || strcmp(tok, "FOR") != 0) {
          fprintf(stderr, "Error parsing.\n");
          exit(1);
        }
        tok = strtok(NULL, " ");
        if(!tok || strcmp(tok, "SITE") != 0) {
          fprintf(stderr, "Error parsing.\n");
          exit(1);
        }
        tok = strtok(NULL, " ");
        if(!tok) break;
        endptr = NULL;
        mbi = strtoimax(tok, &endptr, 10);
        it = tree_lookup(sites, mbi);
        if(tree_it_good(it)) {
          cur_site = tree_it_val(it);
        } else {
          cur_site = malloc(sizeof(site));
          cur_site->bandwidth = 0;
          cur_site->peak_rss = 0;
          cur_site->accesses = 0;
          tree_insert(sites, mbi, cur_site);
        }
      } else if(strcmp(tok, "PEBS") == 0) {
        pebs = 1;
        continue; /* Don't need the rest of this line */
      } else if(strcmp(tok, "END") == 0) {
        mbi = 0;
        pebs = 0;
        continue;
      } else {
        fprintf(stderr, "Found '=====', but no descriptor. Aborting.\n");
        fprintf(stderr, "%s\n", line);
        exit(1);
      }
      len = 0;
      free(line);
      line = NULL;
      continue;
    }

    /* If we're already in a block of results */
    if(mbi) {
      /* We've already gotten the first token, use it */
      if(tok && strcmp(tok, "Average") == 0) {
        tok = strtok(NULL, " ");
        if(tok && strcmp(tok, "bandwidth:") == 0) {
          /* Get the average bandwidth value for this site */
          tok = strtok(NULL, " ");
          endptr = NULL;
          bandwidth = strtof(tok, &endptr);
          cur_site->bandwidth = bandwidth;
        } else {
          fprintf(stderr, "Got 'Average', but no expected tokens. Aborting.\n");
          exit(1);
        }
      } else {
        fprintf(stderr, "In a block of MBI results, but no expected tokens.\n");
        exit(1);
      }
      continue;
    } else if(pebs) {
      /* We're in a block of PEBS results */
      if(tok && (strcmp(tok, "Site") == 0)) {
        /* Get the site number */
        tok = strtok(NULL, " ");
        if(tok) {
          /* Get the site number, then wait for the next line */
          endptr = NULL;
          pebs_site = strtoimax(tok, &endptr, 10);
          it = tree_lookup(sites, pebs_site);
          if(tree_it_good(it)) {
            cur_site = tree_it_val(it);
          } else {
            cur_site = malloc(sizeof(site));
            cur_site->bandwidth = 0;
            cur_site->peak_rss = 0;
            cur_site->accesses = 0;
            tree_insert(sites, mbi, cur_site);
          }
        } else {
          fprintf(stderr, "Got 'Site' but no expected site number. Aborting.\n");
          exit(1);
        }
      } else if(tok && (strcmp(tok, "Totals:") == 0)) {
        /* Ignore the totals */
        continue;
      } else {
        /* Get some information about a site */
        if(tok && (strcmp(tok, "Accesses:") == 0)) {
          tok = strtok(NULL, " ");
          if(!tok) {
            fprintf(stderr, "Got 'Accesses:' but no value. Aborting.\n");
            exit(1);
          }
          endptr = NULL;
          cur_site->accesses = strtoimax(tok, &endptr, 10);
        } else if(tok && (strcmp(tok, "Peak") == 0)) {
          tok = strtok(NULL, " ");
          if(tok && (strcmp(tok, "RSS:") == 0)) {
            tok = strtok(NULL, " ");
            if(!tok) {
              fprintf(stderr, "Got 'Peak RSS:' but no value. Aborting.\n");
              exit(1);
            }
            endptr = NULL;
            cur_site->peak_rss = strtoimax(tok, &endptr, 10);
          } else {
            fprintf(stderr, "Got 'Peak' but not 'RSS:'. Aborting.\n");
            exit(1);
          }
        } else {
          fprintf(stderr, "Got a site number but no expected information. Aborting.\n");
          exit(1);
        }
      }
    }
  }
  free(line);

#if 0
  /* Now sort the sites by accesses/byte or bandwidth/byte */
  if(proftype == 0) {
    /* bandwidth/byte */
    sorted_sites = tree_make_c(siteptr, unsigned, &bandwidth_cmp);
  } else {
    /* accesses/byte */
    sorted_sites = tree_make_c(siteptr, unsigned, &accesses_cmp);
  }
  tree_traverse(sites, it) {
    /* Only insert if the site has a peak_rss and either a bandwidth or an accesses value */
    if((tree_it_val(it)->peak_rss) &&
       (tree_it_val(it)->bandwidth || tree_it_val(it)->accesses)) {
      tree_insert(sorted_sites, tree_it_val(it), tree_it_key(it));
    } else {
      fprintf(stderr, "WARNING: Site %d doesn't have sufficient information to insert.\n", tree_it_key(it));
    }
  }
#endif

  if(captype == 0) {
    /* Figure out cap_bytes from the ratio */
    cap_bytes = get_peak_rss(sites) * cap_float;
    printf("Capacity Ratio: %f\n", cap_float);
  }
  printf("Capacity: %zu bytes\n", cap_bytes);
  printf("Peak RSS: %zu bytes\n", get_peak_rss(sites));

  /* Now run the packing algorithm */
  if(algo == 0) {
    get_knapsack(sites, cap_bytes, proftype);
  } else if(algo == 1) {
    get_hotset(sites);
  } else if(algo == 2) {
    get_thermos(sites);
  }

  /* Clean up */
  tree_traverse(sites, it) {
    free(tree_it_val(it));
  }
	tree_free(sites);
}
