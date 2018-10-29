#pragma once

#include <inttypes.h>
#include "sicm_low.h"
#include "sicm_impl.h"
#include "tree.h"

/* !!!rdspy */
#define READ_TIMES_BUCKET_SIZE (8)
#define READ_TIMES_MAX (80)
#define READ_TIMES_NBUCKETS (READ_TIMES_MAX / READ_TIMES_BUCKET_SIZE)

#include <stdint.h>
#include "tree.h"

typedef void *addr_t;
use_tree(addr_t, unsigned);
typedef struct {} empty_tree_val;
use_tree(unsigned, empty_tree_val);

typedef unsigned long long hist_t[READ_TIMES_NBUCKETS];

typedef struct {
    hist_t * histograms;
    unsigned n_histograms; 
} ThreadReadsInfo;

void ThreadReadsInfo_init(ThreadReadsInfo*);
void ThreadReadsInfo_finish(ThreadReadsInfo*);

typedef struct {
    tree(unsigned, empty_tree_val) used_sites;
    tree(addr_t, unsigned)         site_map;
    void *                         chunks_end;
    hist_t *                       histograms;
    unsigned                       n_histograms;
} SiteReadsAgg;

void SiteReadsAgg_init(SiteReadsAgg*);
void SiteReadsAgg_finish(SiteReadsAgg*);
void SiteReadsAgg_give_histogram(SiteReadsAgg*, ThreadReadsInfo*);
/* !!!end */

enum arena_layout {
  SHARED_ONE_ARENA, /* One arena between all threads */
  EXCLUSIVE_ONE_ARENA, /* One arena per thread */
  SHARED_DEVICE_ARENAS, /* One arena per device */
  EXCLUSIVE_DEVICE_ARENAS, /* One arena per device per thread */
  SHARED_SITE_ARENAS, /* One arena per allocation site */
  EXCLUSIVE_SITE_ARENAS, /* One arena per allocation site per thread */
  INVALID_LAYOUT
};

/* Keeps track of additional information about arenas for profiling */
typedef struct arena_info {
  unsigned index, id;
  sicm_arena arena;
  size_t accesses, rss, peak_rss;
} arena_info;

/* A tree associating site IDs with device pointers.
 * Sites should be bound the device that they're associated with.
 * Filled with guidance from an offline profiling run or with
 * online profiling.
 */
typedef sicm_device * deviceptr;
use_tree(unsigned, deviceptr);
use_tree(deviceptr, int);

/* So we can access these things from profile.c.
 * These variables are defined in src/high/high.c.
 */
extern extent_arr *extents;
extern extent_arr *rss_extents;
extern arena_info **arenas;
extern int should_profile_all, should_profile_one, should_profile_rss;
extern char *profile_one_event, *profile_all_event;
extern int max_index;
extern int max_sample_pages;
extern int sample_freq;
extern int num_imcs, max_imc_len, max_event_len;
extern char **imcs;

#define DEFAULT_ARENA_LAYOUT INVALID_LAYOUT

__attribute__((constructor))
void sh_init();

__attribute__((destructor))
void sh_terminate();

void* sh_alloc_exact(int id, size_t sz);
void* sh_alloc(int id, size_t sz);

void sh_create_extent(void *begin, void *end);

void sh_free(void* ptr);

/* For parsing information about sites */
typedef struct site {
  float bandwidth;
  size_t peak_rss, accesses;
} site;
typedef site * siteptr;
use_tree(unsigned, siteptr);

/* Reads in profiling information from the file pointer, returns
 * a tree containing all sites and their bandwidth, peak RSS,
 * and number of accesses (if applicable).
 */
static inline tree(unsigned, siteptr) sh_parse_site_info(FILE *file) {
  char *line, *tok, *endptr;
  ssize_t len, read;
  long long num_sites, node;
  siteptr cur_site;
  int mbi, pebs, pebs_site, i;
  float bandwidth;
  tree(unsigned, siteptr) sites;
  tree_it(unsigned, siteptr) it;

  sites = tree_make(unsigned, siteptr);

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
            tree_insert(sites, pebs_site, cur_site);
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

  printf("Tree length: %zu\n", tree_len(sites));

  return sites;
}

/* Gets the peak RSS of the whole application by summing the peak_rss of each
 * site
 */
static inline size_t sh_get_peak_rss(tree(unsigned, siteptr) sites) {
  tree_it(unsigned, siteptr) it;
  size_t total;

  total = 0;
  tree_traverse(sites, it) {
    total += tree_it_val(it)->peak_rss;
  }

  return total;
}
