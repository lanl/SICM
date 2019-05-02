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
#include "sicm_high.h"
#include "sicm_parsing.h"
#include "sicm_tree.h"

union metric {
  float band;
  size_t acc;
  double acc_per_sample;
};

use_tree(siteptr, int);

static inline int sizet_cmp(size_t a, size_t b) {
  int retval;
  /* Maximize bandwidth/byte */
  if(a < b) {
    retval = 1;
  } else if(a > b) {
    retval = -1;
  } else {
    retval = 1;
  }
  return retval;
}

static inline int double_cmp(double a, double b) {
  int retval;
  /* Maximize bandwidth/byte */
  if(a < b) {
    retval = 1;
  } else if(a > b) {
    retval = -1;
  } else {
    retval = 1;
  }
  return retval;
}

/* A bunch of comparison functions, used to sort the trees by
 * different metrics. */
int acc_per_sample_cmp(siteptr a, siteptr b) {
  double a_bpb, b_bpb;
  int retval;

  if(a == b) return 0;

  a_bpb = a->acc_per_sample;
  b_bpb = b->acc_per_sample;

  return double_cmp(a_bpb, b_bpb);
}
int accesses_cmp2(siteptr a, siteptr b) {
  double a_bpb, b_bpb;
  int retval;

  if(a == b) return 0;

  a_bpb = (double)a->accesses;
  b_bpb = (double)b->accesses;

  return sizet_cmp(a_bpb, b_bpb);
}
int bandwidth_cmp(siteptr a, siteptr b) {
  double a_bpb, b_bpb;
  int retval;

  if(a == b) return 0;

  a_bpb = ((double)a->bandwidth) / ((double)a->peak_rss);
  b_bpb = ((double)b->bandwidth) / ((double)b->peak_rss);

  return double_cmp(a_bpb, b_bpb);
}
int accesses_cmp(siteptr a, siteptr b) {
  double a_bpb, b_bpb;
  int retval;

  if(a == b) return 0;

  a_bpb = ((double)a->accesses) / ((double)a->peak_rss);
  b_bpb = ((double)b->accesses) / ((double)b->peak_rss);

  return double_cmp(a_bpb, b_bpb);
}


/* Gets the greatest common divisor of all given sites' sizes
 * by just iterating over them and finding the GCD of the current
 * GCD and the current value
 */
size_t get_gcd(tree(int, siteptr) sites) {
  tree_it(int, siteptr) it;
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

  printf("GCD of site sizes is %zu.\n", gcd);
  return gcd;
}

void scale_sites(app_info *info, float scale) {
  tree_it(int, siteptr) it;
  size_t gcd, multiples, total;

  /* First get the GCD of the original sites, adhere to that so that knapsack will still work */
  gcd = get_gcd(info->sites);

  /* Scale each site */
  printf("Scaling sites down by %f.\n", scale);
  total = 0;
  tree_traverse(info->sites, it) {
    tree_it_val(it)->peak_rss *= scale;

    /* Round down to the nearest multiple of the GCD */
    multiples = tree_it_val(it)->peak_rss / gcd;
    tree_it_val(it)->peak_rss = gcd * multiples;
    total += tree_it_val(it)->peak_rss;
  }
  info->site_peak_rss = total;
}

/* Input is a tree of sites and the capacity (in bytes) that you want to fill
 * up to. Outputs the optimal knapsack. Uses dynamic programming combined with
 * an approximation optimization to limit the amount of memory and runtime it
 * uses.
 */
tree(int, siteptr) get_knapsack(tree(int, siteptr) sites, size_t capacity, char proftype) {
  size_t gcd, num_sites, num_weights, i, j, weight;
  union metric **table, a, b;
  tree(int, siteptr) knapsack;
  tree_it(int, siteptr) it;

  /* Create a matrix with 'i' rows and 'j' columns, where 'i' is the number of
   * sites and 'j' is the number of weights (multiples of the gcd, though). We
   * can find 'j' by dividing the peak RSS of the whole application by the GCD.
   */
  gcd = get_gcd(sites);
  num_sites = tree_len(sites);
  num_weights = (capacity / gcd) + 1;

  printf("There are %zu distinct weights in the knapsack.\n", num_weights);
  printf("Allocating %zu bytes for the knapsack.\n", sizeof(union metric) * num_weights * (num_sites + 1));
  table = malloc(sizeof(union metric *) * (num_sites + 1));
  for(i = 0; i <= num_sites; i++) {
    table[i] = calloc(num_weights, sizeof(union metric));
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
        /* Two cases: MBI and PEBS */
        a = table[i - 1][j];
        if(proftype == 0) {
          b.band = table[i - 1][j - weight].band + tree_it_val(it)->bandwidth;
          if(a.band > b.band) {
            table[i][j] = a;
          } else {
            table[i][j] = b;
          }
        } else if(proftype == 1) {
          b.acc = table[i - 1][j - weight].acc + tree_it_val(it)->accesses;
          if(a.acc > b.acc) {
            table[i][j] = a;
          } else {
            table[i][j] = b;
          }
        } else if(proftype == 2) {
          b.acc_per_sample = table[i - 1][j - weight].acc_per_sample + tree_it_val(it)->acc_per_sample;
          if(a.acc_per_sample > b.acc_per_sample) {
            table[i][j] = a;
          } else {
            table[i][j] = b;
          }
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
  knapsack = tree_make(int, siteptr);
  it = tree_last(sites);
  i = num_sites;
  j = num_weights - 1;
  while(j > 0) {
    if(!tree_it_good(it) || (i == 0)) {
      break;
    }
    if(table[i][j].acc != table[i - 1][j].acc) {
      tree_insert(knapsack, tree_it_key(it), tree_it_val(it));
      j = j - (tree_it_val(it)->peak_rss / gcd);
    }
    i--;
    tree_it_prev(it);
  }

  for(i = 0; i <= num_sites; i++) {
    free(table[i]);
  }
  free(table);

  return knapsack;
}

/* Input is a tree of sites and the capacity (in bytes) that you want to fill
 * up to. Outputs a greedy hotset.
 */
tree(int, siteptr) get_filtered_hotset(tree(int, siteptr) sites, size_t capacity, char proftype, union metric total_value) {
  tree(siteptr, int) sorted_sites;
  tree(int, siteptr) ret;
  tree_it(int, siteptr) it;
  tree_it(siteptr, int) sit;
  char break_next_site;
  size_t packed_size;

  ret = tree_make(int, siteptr);

  /* Now sort the sites by accesses/byte or bandwidth/byte */
  if(proftype == 0) {
    /* bandwidth/byte */
    sorted_sites = tree_make_c(siteptr, int, &bandwidth_cmp);
  } else if(proftype == 1) {
    /* accesses/byte */
    sorted_sites = tree_make_c(siteptr, int, &accesses_cmp);
  } else if(proftype == 2) {
    sorted_sites = tree_make_c(siteptr, int, &acc_per_sample_cmp);
  }

  tree_traverse(sites, it) {
    /* Only insert if the site has a peak_rss value */
    if(tree_it_val(it)->peak_rss) {
      tree_insert(sorted_sites, tree_it_val(it), tree_it_key(it));
    } else {
      fprintf(stderr, "WARNING: Site %d doesn't have a peak RSS.\n", tree_it_key(it));
    }
  }

  printf("Sorted sites:\n");
  tree_traverse(sorted_sites, sit) {
    if(proftype == 0) {
      printf("%d: %f %zu %f\n", tree_it_val(sit), tree_it_key(sit)->bandwidth, tree_it_key(sit)->peak_rss, ((double)tree_it_key(sit)->bandwidth) / ((double)tree_it_key(sit)->peak_rss));
    } else if(proftype == 1) {
      printf("%d: %zu %zu %f\n", tree_it_val(sit), tree_it_key(sit)->accesses, tree_it_key(sit)->peak_rss, ((double)tree_it_key(sit)->accesses) / ((double)tree_it_key(sit)->peak_rss));
    } else if(proftype == 2) {
      printf("%d: %.10f %zu\n", tree_it_val(sit), tree_it_key(sit)->acc_per_sample, tree_it_key(sit)->peak_rss);
    }
  }

  /* Now iterate over the sorted sites and add them until we overflow */
	break_next_site = 0;
  packed_size = 0;
  tree_traverse(sorted_sites, sit) {
    
    /* First just grab the more important sites, even if they're larger */
    if(proftype == 0) {
      if(tree_it_key(sit)->bandwidth / total_value.band > 0.005) {
        packed_size += tree_it_key(sit)->peak_rss;
        tree_insert(ret, tree_it_val(sit), tree_it_key(sit));
      }
    } else if(proftype == 1) {
      if(tree_it_key(sit)->accesses / total_value.acc > 0.005) {
        printf("Adding %d\n", tree_it_val(sit));
        packed_size += tree_it_key(sit)->peak_rss;
        tree_insert(ret, tree_it_val(sit), tree_it_key(sit));
      } else {
        printf("SKIPPING %d\n", tree_it_val(sit));
      }
    } else if(proftype == 2) {
      if(tree_it_key(sit)->acc_per_sample / total_value.acc_per_sample > 0.005) {
        printf("Adding %d\n", tree_it_val(sit));
        packed_size += tree_it_key(sit)->peak_rss;
        tree_insert(ret, tree_it_val(sit), tree_it_key(sit));
      } else {
        printf("SKIPPING %d\n", tree_it_val(sit));
      }
    }

		/* If we're over capacity, break. We've already added the site,
		 * so we overflow by exactly one site. */
		if(packed_size > capacity) {
			break;
		}
	}

  if(packed_size < capacity) {
    tree_traverse(sorted_sites, sit) {
      packed_size += tree_it_key(sit)->peak_rss;
      tree_insert(ret, tree_it_val(sit), tree_it_key(sit));
      printf("Backfilling %d\n", tree_it_val(sit));

      /* If we're over capacity, break. We've already added the site,
       * so we overflow by exactly one site. */
      if(packed_size > capacity) {
        break;
      }
    }
  }

	return ret;
}

/* Input is a tree of sites and the capacity (in bytes) that you want to fill
 * up to. Outputs a greedy hotset.
 */
tree(int, siteptr) get_hotset(tree(int, siteptr) sites, size_t capacity, char proftype) {
  tree(siteptr, int) sorted_sites;
  tree(int, siteptr) ret;
  tree_it(int, siteptr) it;
  tree_it(siteptr, int) sit;
  char break_next_site;
  size_t packed_size;

  ret = tree_make(int, siteptr);

  /* Now sort the sites by accesses/byte or bandwidth/byte */
  if(proftype == 0) {
    /* bandwidth/byte */
    sorted_sites = tree_make_c(siteptr, int, &bandwidth_cmp);
  } else if(proftype == 1) {
    /* accesses/byte */
    sorted_sites = tree_make_c(siteptr, int, &accesses_cmp);
  } else if(proftype == 2) {
    sorted_sites = tree_make_c(siteptr, int, &acc_per_sample_cmp);
  }
  tree_traverse(sites, it) {
    /* Only insert if the site has a peak_rss value */
    if(tree_it_val(it)->peak_rss) {
      tree_insert(sorted_sites, tree_it_val(it), tree_it_key(it));
    } else {
      fprintf(stderr, "WARNING: Site %d doesn't have a peak RSS.\n", tree_it_key(it));
    }
  }

  printf("Sorted sites:\n");
  tree_traverse(sorted_sites, sit) {
    if(proftype == 0) {
      printf("%d: %f %zu %f\n", tree_it_val(sit), tree_it_key(sit)->bandwidth, tree_it_key(sit)->peak_rss, ((double)tree_it_key(sit)->bandwidth) / ((double)tree_it_key(sit)->peak_rss));
    } else if(proftype == 1) {
      printf("%d: %zu %zu %f\n", tree_it_val(sit), tree_it_key(sit)->accesses, tree_it_key(sit)->peak_rss, ((double)tree_it_key(sit)->accesses) / ((double)tree_it_key(sit)->peak_rss));
    } else if(proftype == 2) {
      printf("%d: %.10f %zu\n", tree_it_val(sit), tree_it_key(sit)->acc_per_sample, tree_it_key(sit)->peak_rss);
    }
  }

  /* Now iterate over the sorted sites and add them until we overflow */
	break_next_site = 0;
  packed_size = 0;
  tree_traverse(sorted_sites, sit) {
		packed_size += tree_it_key(sit)->peak_rss;
		tree_insert(ret, tree_it_val(sit), tree_it_key(sit));

		/* If we're over capacity, break. We've already added the site,
		 * so we overflow by exactly one site. */
		if(packed_size > capacity) {
			break;
		}
	}

	return ret;
}

/* Returns a filled Thermos hotset given a tree of sites */
tree(int, siteptr) get_thermos(tree(int, siteptr) sites, size_t capacity, char proftype) {
  tree(siteptr, int) sorted_sites;
  tree(int, siteptr) ret;
  tree_it(int, siteptr) it;
  tree_it(siteptr, int) sit;
  tree_it(int, siteptr) tmp_sit;
  char break_next_site;
  size_t packed_size, site_size, over, tmp_size, tmp_accs, site_accs;
	float site_band, tmp_band;
  double site_acc_per_sample, tmp_acc_per_sample;
	int site;

  ret = tree_make(int, siteptr);

  /* Now sort the sites by accesses/byte or bandwidth/byte */
  if(proftype == 0) {
    /* bandwidth/byte */
    sorted_sites = tree_make_c(siteptr, int, &bandwidth_cmp);
  } else if(proftype == 1) {
    /* accesses/byte */
    sorted_sites = tree_make_c(siteptr, int, &accesses_cmp);
  } else if(proftype == 2) {
    sorted_sites = tree_make_c(siteptr, int, &acc_per_sample_cmp);
  }
  tree_traverse(sites, it) {
    /* Only insert if the site has a peak_rss and either a bandwidth or an accesses value */
    if(tree_it_val(it)->peak_rss) {
      tree_insert(sorted_sites, tree_it_val(it), tree_it_key(it));
    } else {
      fprintf(stderr, "WARNING: Site %d doesn't have a peak RSS.\n", tree_it_key(it));
    }
  }

	break_next_site = 0;
  packed_size = 0;
  tree_traverse(sorted_sites, sit) {
		site = tree_it_val(sit);
		site_size = tree_it_key(sit)->peak_rss;
		if(proftype == 0) {
			site_band = tree_it_key(sit)->bandwidth;
		} else if(proftype == 1) {
			site_accs = tree_it_key(sit)->accesses;
		} else if(proftype == 2) {
			site_acc_per_sample = tree_it_key(sit)->acc_per_sample;
    }

		if((packed_size + site_size) > capacity) {
			/* Store how much over the capacity we are */
			over = packed_size + site_size - capacity;
			/* printf("We're over by %zu bytes.\n", over); */

			/* Iterate over the current hotset, and add up the
			 * value and size of the sites that would be offset
			 * by `over` bytes being pushed out of the upper tier. */
			tmp_size = 0;
			tmp_accs = 0;
			tmp_band = 0.0;
      tree_traverse(ret, tmp_sit) {
				tmp_size += tree_it_val(tmp_sit)->peak_rss;
				if(proftype == 0) {
					tmp_band += tree_it_val(tmp_sit)->bandwidth;
				} else if(proftype == 1) {
					tmp_accs += tree_it_val(tmp_sit)->accesses;
				} else if(proftype == 2) {
					tmp_acc_per_sample += tree_it_val(tmp_sit)->acc_per_sample;
        }
				if(tmp_size > over) {
					break;
				}
      }
			/*
			printf("The temporary accesses is %zu\n", tmp_accs);
			printf("The site's accesses is %zu\n", site_accs);
			*/

			/* If the value of the current site is greater than the value
			 * of the data that would be pushed out of the upper tier, add
			 * that site to the hotset. */
			if(proftype == 0) {
				if(site_band > tmp_band) {
					packed_size += tree_it_key(sit)->peak_rss;
					tree_insert(ret, tree_it_val(sit), tree_it_key(sit));
				}
			} else if(proftype == 1) { 
				if(site_accs > tmp_accs) {
					packed_size += tree_it_key(sit)->peak_rss;
					tree_insert(ret, tree_it_val(sit), tree_it_key(sit));
				}
			} else if(proftype == 2) {
				if(site_acc_per_sample > tmp_acc_per_sample) {
					packed_size += tree_it_key(sit)->peak_rss;
					tree_insert(ret, tree_it_val(sit), tree_it_key(sit));
				}
      }
		} else {
			/* Add the site to the hotset */
			/* printf("Not over yet. Adding site %u.\n", tree_it_val(sit)); */
			packed_size += tree_it_key(sit)->peak_rss;
			tree_insert(ret, tree_it_val(sit), tree_it_key(sit));
		}
	}

	return ret;
}

/* Reads in profiling information from stdin, then runs the packing algorithm
 * based on arguments. Prints the hotset to stdout.
 */
int main(int argc, char **argv) {
  char proftype, algo, captype, *endptr;
  size_t cap_bytes, chosen_weight, total_weight, tot_peak_rss, gcd;
  union metric chosen_value, total_value;
  long long node;
  float cap_float, scale;
  tree(int, siteptr) sites, chosen_sites;
  tree_it(int, siteptr) it;
  app_info *info;

  /* Read in the arguments */
  if(argc != 7) {
    fprintf(stderr, "USAGE: ./hotset proftype algo captype cap node\n");
    fprintf(stderr, "proftype: band, acc, or acc_per_sample, the type of profiling.\n");
    fprintf(stderr, "algo: knapsack, hotset, or thermos. The packing algorithm.\n");
    fprintf(stderr, "captype: ratio or constant. The type of capacity.\n");
    fprintf(stderr, "cap: the capacity. A float 0-1 if captype is 'ratio', or a\n");
    fprintf(stderr, "  constant number of bytes otherwise.\n");
    fprintf(stderr, "node: the node that chosen sites should be associated with.\n");
    fprintf(stderr, "tot_peak_rss: the peak RSS of the run, to be used to scale the site RSS. Set to 0 for no scaling.\n");
    exit(1);
  }
  if(strcmp(argv[1], "band") == 0) {
    proftype = 0;
  } else if(strcmp(argv[1], "acc") == 0) {
    proftype = 1;
  } else if(strcmp(argv[1], "acc_per_sample") == 0) {
    proftype = 2;
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
  } else if(strcmp(argv[2], "filtered_hotset") == 0) {
    algo = 3;
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
  endptr = NULL;
  node = strtoimax(argv[5], &endptr, 10);
  if(node > INT_MAX) {
    fprintf(stderr, "The node that you specified is greater than an integer can store. Aborting.\n");
    exit(1);
  }
  endptr = NULL;
  tot_peak_rss = strtoumax(argv[6], &endptr, 10);

  info = sh_parse_site_info(stdin);

  if(captype == 0) {
    /* Figure out cap_bytes from the ratio */
    cap_bytes = info->site_peak_rss * cap_float;
  }

  /* Scale the sites' peak RSS down according to the peak RSS of the whole run */
  if(tot_peak_rss != 0) {
    /* Ratio of:
     * 1. The sum of all sites' peak RSS
     * 2. The actual peak RSS of the whole application
     */
    printf("Scaling from a peak RSS of %zu to a peak RSS of %zu.\n", info->site_peak_rss, tot_peak_rss);
    scale = ((float)tot_peak_rss) / ((float) info->site_peak_rss);
    scale_sites(info, scale);
  }

  /* Calculate what we had to choose from */
  total_weight = 0;
  total_value.acc = 0;
  total_value.band = 0;
  tree_traverse(info->sites, it) {
    total_weight += tree_it_val(it)->peak_rss;
    if(proftype == 0) { 
      total_value.band += tree_it_val(it)->bandwidth;
    } else if(proftype == 1) {
      total_value.acc += tree_it_val(it)->accesses;
    } else if(proftype == 2) {
      total_value.acc_per_sample += tree_it_val(it)->acc_per_sample;
    }
  }

  /* Now run the packing algorithm */
  if(algo == 0) {
    chosen_sites = get_knapsack(info->sites, cap_bytes, proftype);
  } else if(algo == 1) {
    chosen_sites = get_hotset(info->sites, cap_bytes, proftype);
  } else if(algo == 2) {
    chosen_sites = get_thermos(info->sites, cap_bytes, proftype);
  } else if(algo == 3) {
    chosen_sites = get_filtered_hotset(info->sites, cap_bytes, proftype, total_value);
  }

  /* Calculate what we chose */
  chosen_weight = 0;
  chosen_value.acc = 0;
  chosen_value.band = 0;
  tree_traverse(chosen_sites, it) {
    chosen_weight += tree_it_val(it)->peak_rss;
    if(proftype == 0) { 
      chosen_value.band += tree_it_val(it)->bandwidth;
    } else if(proftype == 1) {
      chosen_value.acc += tree_it_val(it)->accesses;
    } else if(proftype == 2) {
      chosen_value.acc_per_sample += tree_it_val(it)->acc_per_sample;
    }
  }

  /* Print out the calculated results */
  printf("===== GUIDANCE =====\n");
  tree_traverse(chosen_sites, it) {
    printf("%u %d\n", tree_it_key(it), (int) node);
  }
  printf("===== END GUIDANCE =====\n");
  if(algo == 0) {
    printf("Strategy: Knapsack\n");
  } else if(algo == 1) {
    printf("Strategy: Hotset\n");
  } else if(algo == 2) {
    printf("Strategy: Thermos\n");
  }
  printf("Used capacity: %zu/%zu bytes\n", chosen_weight, total_weight);
  if(proftype == 0) {
    printf("Value: %f/%f\n", chosen_value.band, total_value.band);
  } else if(proftype == 1) {
    printf("Value: %zu/%zu\n", chosen_value.acc, total_value.acc);
  } else if(proftype == 2) {
    printf("Value: %.10f/%.10f\n", chosen_value.acc_per_sample, total_value.acc_per_sample);
  }
  printf("Capacity: %zu bytes\n", cap_bytes);
  printf("Peak RSS: %zu bytes\n", info->site_peak_rss);

  /* Clean up */
  tree_traverse(info->sites, it) {
    free(tree_it_val(it));
  }
	tree_free(info->sites);
  free(info);
  tree_free(chosen_sites);
}
