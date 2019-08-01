/*
 * Contains code for parsing output of the high-level interface.
 * Reads information about each allocation site into a structure.
 */

#include <inttypes.h>
#include <limits.h>
#include <stdio.h>
#include "sicm_tree.h"

/* Holds information about a single site and a single type of profiling */
typedef struct event {
  char *name;
  size_t total, peak;
} event;

/* Holds information on a single site */
typedef struct site {
  size_t num_events;
  event *events;
  int id;
} site;
typedef site * siteptr;

/* Tree of sites */
use_tree(int, siteptr);
typedef struct app_info {
  tree(int, siteptr) sites;
  event *events;
  size_t num_events;
} app_info;

/* Creates a new event and adds it to the app_info structure, as
 * well as the cur_sites sites */
static inline void create_event(app_info *info, siteptr *cur_sites, size_t num_sites, char *eventstr) {
  size_t i;
  char seen;
  siteptr cur_site;

  /* See if we've seen this event before */
  seen = 0;
  for(i = 0; i < info->num_events; i++) {
    if(strncmp(info->events[i].name, eventstr, 64) == 0) {
      seen = 1;
    }
  }

  if(seen) {
    /* Store this event for the whole application */
    info->num_events++;
    info->events = (event *) realloc(info->events,
                                     sizeof(event) * info->num_events);
    info->events[info->num_events - 1].name = (char *) malloc(sizeof(char) * 64);
    strcpy(info->events[info->num_events - 1].name, eventstr);
    info->events[info->num_events - 1].total = 0;
    info->events[info->num_events - 1].peak = 0;
  }

  /* Iterate over the given sites */
  for(i = 0; i < num_sites; i++) {
    /* Allocate room for the new event */
    cur_site = cur_sites[i];
    cur_site->num_events++;
    cur_site->events = (event *) realloc(cur_site->events, 
                                         sizeof(event) * cur_site->num_events);
    cur_site->events[cur_site->num_events - 1].name = (char *) malloc(sizeof(char) * 64);
    strcpy(cur_site->events[cur_site->num_events - 1].name, eventstr);
    cur_site->events[cur_site->num_events - 1].total = 0;
    cur_site->events[cur_site->num_events - 1].peak = 0;
  }
}

/* "tp" is 0 for total, 1 for peak */
static inline void add_event_total(app_info *info, siteptr *cur_sites, size_t num_sites, size_t val, char tp) {
  size_t i;
  siteptr cur_site;

  /* Find this event in the app_info struct, add up the total */
  cur_site = cur_sites[0];
  for(i = 0; i < info->num_events; i++) {
    if(strncmp(cur_site->events[cur_site->num_events].name, info->events[i].name, 64) == 0) {
      if(tp == 0) {
        info->events[i].total += val;
      } else {
        info->events[i].peak += val;
      }
      break;
    }
  }

  for(i = 0; i < num_sites; i++) {
    cur_site = cur_sites[i];
    if(tp == 0) {
      cur_site->events[cur_site->num_events - 1].total = val;
    } else {
      cur_site->events[cur_site->num_events - 1].total = val;
    }
  }
}

/* Reads in profiling information from the file pointer, returns
 * a tree containing all sites and their bandwidth, peak RSS,
 * and number of accesses (if applicable).
 */
static inline app_info *sh_parse_site_info(FILE *file) {
  char *line, in_block, *tok, in_event;
  size_t len, val, num_sites;
  double val_double;
  ssize_t read;
  siteptr *cur_sites; /* An array of site pointers */
  siteptr cur_site;
  int site_id, num_tok, i, n;
  float bandwidth, seconds;
  tree_it(int, siteptr) it;
  app_info *info;

  info = (app_info *)malloc(sizeof(app_info));
  info->sites = tree_make(int, siteptr);
  info->num_events = 0;
  info->events = NULL;

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
  in_event = 0;

  while(read = getline(&line, &len, file) != -1) {

    if(in_block == 0) {
      /* We're not in any results blocks, try to get into one */

      if(strncmp(line, "===== PEBS RESULTS =====\n", 25) == 0) {
        /* Found some PEBS results */
        in_block = 2;
        in_event = 0;
      }
    } else if(in_block == 1) {
    } else if(in_block == 2) {
      /* See if this is the start of a site's profiling */
      num_tok = sscanf(line,
                      "%d sites: %d",
                      &i,
                      &n);
      if(num_tok == 2) {
        num_tok = sscanf(line,
                        "%zu sites: %d",
                        &num_sites,
                        &n);
        fprintf(stderr, "Reading in %d sites:\n", num_sites);
        fprintf(stderr, "%s\n", line);
        cur_sites = (siteptr *) malloc(sizeof(siteptr) * num_sites);
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
              /* We already created this site */
              cur_sites[i] = tree_it_val(it);
            } else {
              /* Create the site */
              cur_sites[i] = (siteptr) malloc(sizeof(site));
              cur_sites[i]->events = NULL;
              cur_sites[i]->num_events = 0;
              cur_sites[i]->id = site_id;
              tree_insert(info->sites, site_id, cur_sites[i]);
              fprintf(stderr, "Creating site %d\n", site_id);
            }
          } else {
            break;
          }
          tok = strtok(NULL, " ");
          i++;
        }
        fprintf(stderr, "Sites: ");
        for(i = 0; i < num_sites; i++) {
          fprintf(stderr, "%d ", cur_sites[i]->id);
        }
        continue;
      }

      /* See if this line defines a new event */
      tok = NULL;
      if(strncmp(line, "  Event: ", 9) == 0) {
        tok = (char *) malloc(sizeof(char) * 64);
        num_tok = sscanf(line, "  Event: %s\n", tok);
      } else if(strncmp(line, "  Extents size:", 15) == 0) {
        const char *tmp = "extent_size";
        tok = (char *) malloc(sizeof(char) * 64);
        strcpy(tok, tmp);
      }
      if(tok) {
        /* Triggered if we found a new event above, event name
         * is stored in tok */
        in_event = 1;
        create_event(info, cur_sites, num_sites, tok);

        continue;

      /* If we didn't find a new event above, then we're most likely in an event */
      } else if(in_event) {
        num_tok = sscanf(line, "  Total: %zu\n", &val);
        if(num_tok == 1) {
          add_event_total(info, cur_sites, num_sites, val, 0);
          continue;
        }

        /* Get peak number that this event got to this arena */
        num_tok = sscanf(line, "  Peak: %zu\n", &val);
        if(num_tok == 1) {
          add_event_total(info, cur_sites, num_sites, val, 1);
          continue;
        }
        continue;
      }
      
      if(strncmp(line, "===== END PEBS RESULTS =====\n", 32) == 0) {
        in_block = 0;
        in_event = 0;
        free(cur_sites);
        continue;
      }
    }
  }
  free(line);

  return info;
}
