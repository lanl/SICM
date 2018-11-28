/*
 * Contains code for parsing output of the high-level interface.
 * Reads information about each allocation site into a structure,
 * as well as information that tools like /usr/bin/time output.
 */

#pragma once

#include "tree.h"

/* For parsing information about sites */
typedef struct site {
  float bandwidth;
  size_t peak_rss, accesses;
} site;
typedef site * siteptr;
use_tree(unsigned, siteptr);
typedef struct app_info {
  size_t peak_rss;
  tree(unsigned, siteptr) sites;
} app_info;

/* Reads in profiling information from the file pointer, returns
 * a tree containing all sites and their bandwidth, peak RSS,
 * and number of accesses (if applicable).
 */
static inline app_info *sh_parse_site_info(FILE *file) {
  char *line, *tok;
  ssize_t len, read;
  long long num_sites, node;
  siteptr cur_site;
  int mbi, pebs, pebs_site, i;
  float bandwidth;
  tree_it(unsigned, siteptr) it;
  app_info *info;

  info = malloc(sizeof(app_info));
  info->sites = tree_make(unsigned, siteptr);
  info->peak_rss = 0;

  if(!file) {
    fprintf(stderr, "Invalid file pointer. Aborting.\n");
    exit(1);
  }

  /* Read in from stdin and fill in the tree with the sites */
  num_sites = 0;
  mbi = 0;
  pebs = 0;
  pebs_site = 0;
  line = NULL;
  len = 0;
  while(read = getline(&line, &len, file) != -1) {

    tok = strtok(line, " \t");
    if(!tok) break;

    /* Find the beginning or end of some results */
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
        mbi = strtoimax(tok, NULL, 10);
        it = tree_lookup(info->sites, mbi);
        if(tree_it_good(it)) {
          cur_site = tree_it_val(it);
        } else {
          cur_site = malloc(sizeof(site));
          cur_site->bandwidth = 0;
          cur_site->peak_rss = 0;
          cur_site->accesses = 0;
          tree_insert(info->sites, mbi, cur_site);
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
          bandwidth = strtof(tok, NULL);
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
          pebs_site = strtoimax(tok, NULL, 10);
          it = tree_lookup(info->sites, pebs_site);
          if(tree_it_good(it)) {
            cur_site = tree_it_val(it);
          } else {
            cur_site = malloc(sizeof(site));
            cur_site->bandwidth = 0;
            cur_site->peak_rss = 0;
            cur_site->accesses = 0;
            tree_insert(info->sites, pebs_site, cur_site);
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
          cur_site->accesses = strtoimax(tok, NULL, 10);
        } else if(tok && (strcmp(tok, "Peak") == 0)) {
          tok = strtok(NULL, " ");
          if(tok && (strcmp(tok, "RSS:") == 0)) {
            tok = strtok(NULL, " ");
            if(!tok) {
              fprintf(stderr, "Got 'Peak RSS:' but no value. Aborting.\n");
              exit(1);
            }
            cur_site->peak_rss = strtoimax(tok, NULL, 10);
          } else {
            fprintf(stderr, "Got 'Peak' but not 'RSS:'. Aborting.\n");
            exit(1);
          }
        } else {
          fprintf(stderr, "Got a site number but no expected information. Aborting.\n");
          exit(1);
        }
      }
    } else {
      /* Parse the output of /usr/bin/time to get peak RSS */
      if(strcmp(tok, "Maximum") == 0) {
        tok = strtok(NULL, " ");
        if(!tok || !(strcmp(tok, "resident") == 0)) {
          continue;
        }
        tok = strtok(NULL, " ");
        if(!tok || !(strcmp(tok, "set") == 0)) {
          continue;
        }
        tok = strtok(NULL, " ");
        if(!tok || !(strcmp(tok, "size") == 0)) {
          continue;
        }
        tok = strtok(NULL, " ");
        if(!tok || !(strcmp(tok, "(kbytes):") == 0)) {
          continue;
        }
        tok = strtok(NULL, " ");
        if(!tok) {
          continue;
        }
        info->peak_rss = strtoimax(tok, NULL, 10) * 1024; /* it's in kilobytes */
      }
    }
  }
  free(line);

  return info;
}

/* Gets the peak RSS of the whole application by summing the peak_rss of each
 * site
 */
static inline size_t sh_get_peak_rss(app_info *info) {
  tree_it(unsigned, siteptr) it;
  size_t total;

  total = info->peak_rss;

  if(!total) {
    tree_traverse(info->sites, it) {
      total += tree_it_val(it)->peak_rss;
    }
  }

  return total;
}
