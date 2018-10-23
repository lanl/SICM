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

/* Returns a filled knapsack given a tree of sites and a maximum capacity */
tree(unsigned, siteptr) *get_knapsack(tree(siteptr, unsigned) *sites) {
  
}

/* Returns a filled hotset given a tree of sites */
tree(unsigned, siteptr) *get_hotset(tree(siteptr, unsigned) *sites) {
}

/* Returns a filled Thermos hotset given a tree of sites */
tree(unsigned, siteptr) *get_thermos(tree(siteptr, unsigned) *sites) {
}

/* Reads in profiling information from stdin, then runs the packing algorithm
 * based on arguments. Prints the hotset to stdout.
 */
int main(int argc, char **argv) {
  char *line, *tok, *endptr, proftype, algo;
  ssize_t len, read;
  long long num_sites;
  siteptr cur_site;
  int mbi, pebs, pebs_site, i;
  float bandwidth;
  tree(unsigned, siteptr) sites;
  tree(siteptr, unsigned) sorted_sites;
  tree_it(unsigned, siteptr) it;
  tree_it(siteptr, unsigned) sorted_it;

  /* Read in the arguments */
  if(argc != 3) {
    fprintf(stderr, "USAGE: ./hotset proftype algo\n");
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
          printf("Inserting %d\n", mbi);
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
            printf("Inserting\n");
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

  /* Print the tree */
  fprintf(stderr, "Final tree:\n");
  tree_traverse(sorted_sites, sorted_it) {
    fprintf(stderr, "  %u:\n", tree_it_val(sorted_it));
    fprintf(stderr, "    %f\n", tree_it_key(sorted_it)->bandwidth);
    fprintf(stderr, "    %zu\n", tree_it_key(sorted_it)->accesses);
    fprintf(stderr, "    %zu\n", tree_it_key(sorted_it)->peak_rss);
  }

  /* Now run the packing algorithm */
  if(algo == 0) {
    get_knapsack(&sorted_sites);
  } else if(algo == 1) {
    get_hotset(&sorted_sites);
  } else if(algo == 2) {
    get_thermos(&sorted_sites);
  }

  /* Clean up */
	tree_free(sites);
  tree_free(sorted_sites);
}
