/*
 * Contains code for parsing output of the high-level interface.
 * Reads information about each allocation site into a structure.
 */

#include <inttypes.h>
#include <limits.h>
#include <stdio.h>
#include "sicm_tree.h"

/* Holds information on a single site */
typedef struct site {
	float bandwidth;
	size_t peak_rss, accesses;
  double acc_per_sample;
} site;
typedef site * siteptr;

/* Tree of sites */
use_tree(int, siteptr);
typedef struct app_info {
	size_t num_pebs_sites, num_mbi_sites, site_peak_rss;
	tree(int, siteptr) sites;
} app_info;

/* Reads in profiling information from the file pointer, returns
 * a tree containing all sites and their bandwidth, peak RSS,
 * and number of accesses (if applicable).
 */
static inline app_info *sh_parse_site_info(FILE *file) {
	char *line, in_block, *tok;
	size_t len, val;
  double val_double;
  ssize_t read;
	siteptr *cur_sites; /* An array of site pointers */
	int site_id, num_tok, num_sites, i;
	float bandwidth, seconds;
	tree_it(int, siteptr) it;
	app_info *info;

	info = malloc(sizeof(app_info));
	info->sites = tree_make(int, siteptr);
	info->num_pebs_sites = 0;
	info->num_mbi_sites = 0;

	if(!file) {
		fprintf(stderr, "Invalid file pointer. Aborting.\n");
		exit(1);
	}

	/* Read in from stdin and fill in the tree with the sites */
  cur_sites = NULL;
  site_id = 0;
	line = NULL;
	len = 0;
  in_block = 0; /* 0 if not in results block,
                   1 if in MBI results block,
                   2 if in PEBS results block */

	while(read = getline(&line, &len, file) != -1) {

    if(in_block == 0) {
      /* Try to find the beginning of some results */
      num_tok = sscanf(line,
                      "===== MBI RESULTS FOR SITE %d =====\n",
                      &site_id);
      if(num_tok == 1) {
        /* Found some MBI results */
        in_block = 1;
        it = tree_lookup(info->sites, site_id);
        cur_sites = malloc(sizeof(siteptr));
        if(tree_it_good(it)) {
          cur_sites[0] = tree_it_val(it);
        } else {
          cur_sites[0] = malloc(sizeof(site));
          cur_sites[0]->bandwidth = 0;
          cur_sites[0]->peak_rss = 0;
          cur_sites[0]->accesses = 0;
          cur_sites[0]->acc_per_sample = 0.0;
          tree_insert(info->sites, site_id, cur_sites[0]);
          info->num_mbi_sites++;
        }
        continue;
      }
      if(strncmp(line, "===== PEBS RESULTS =====\n", 25) == 0) {
        /* Found some PEBS results */
        in_block = 2;
      }
    } else if(in_block == 1) {
      /* If we're in a block of MBI results */
      num_tok = sscanf(line, 
                      "Average bandwidth: %f MB/s\n",
                      &(cur_sites[0]->bandwidth));
      if(num_tok == 1) {
        continue;
      }
      num_tok = sscanf(line,
                      "Peak RSS: %zu\n",
                      &(cur_sites[0]->peak_rss));
      if(num_tok == 1) {
        continue;
      }
      if(strncmp(line, "===== END MBI RESULTS =====\n", 30) == 0) {
        in_block = 0;
        /* Deallocate the array, but not each element (those are in the tree). */
        free(cur_sites);
        continue;
      }
    } else if(in_block == 2) {
      /* We're in a block of PEBS results */
      num_tok = sscanf(line,
                      "%d sites:",
                      &num_sites);
      if(num_tok == 1) {
        cur_sites = malloc(sizeof(siteptr) * num_sites);
        /* Iterate over the site IDs and read them in */
        tok = strtok(line, " ");
        tok = strtok(NULL, " ");
        tok = strtok(NULL, " ");
        i = 0;
        while(tok != NULL) {
          num_tok = sscanf(tok, "%d", &site_id);
          if(num_tok == 1) {
            it = tree_lookup(info->sites, site_id);
            if(tree_it_good(it)) {
              cur_sites[i] = tree_it_val(it);
            } else {
              cur_sites[i] = malloc(sizeof(site));
              cur_sites[i]->bandwidth = 0;
              cur_sites[i]->peak_rss = 0;
              cur_sites[i]->accesses = 0;
              cur_sites[i]->acc_per_sample = 0.0;
              tree_insert(info->sites, site_id, cur_sites[i]);
              info->num_pebs_sites++;
            }
          } else {
            break;
          }
          tok = strtok(NULL, " ");
          i++;
        }
      }

      /* Get number of PEBS accesses */
      num_tok = sscanf(line, "  Accesses: %zu\n", &val);
      if(num_tok == 1) {
        /* This value applies to all sites in the arena */
        for(i = 0; i < num_sites; i++) {
          cur_sites[i]->accesses = val;
        }
      }

      /* Get Peak RSS */
      num_tok = sscanf(line, "  Peak RSS: %zu\n", &val);
      if(num_tok == 1) {
        /* This value applies to all sites in the arena */
        for(i = 0; i < num_sites; i++) {
          cur_sites[i]->peak_rss = val;
        }
      }

      /* If we're at the end of a block of PEBS results */
      if(strncmp(line, "===== END PEBS RESULTS =====\n", 32) == 0) {
        in_block = 0;
        free(cur_sites);
        continue;
      }
    }
	}
	free(line);

  info->site_peak_rss = 0;
  tree_traverse(info->sites, it) {
    info->site_peak_rss += tree_it_val(it)->peak_rss;
  }

	return info;
}
