#pragma once
/*
 * Contains code for parsing output of the high-level interface.
 * Also outputs profiling information to a file.
 */

#include <inttypes.h>
#include <limits.h>
#include <stdio.h>
#include "sicm_tree.h"
#include "sicm_profile.h"

/* Iterates over the application_profile
   structure and prints it out so that it can
 * be seamlessly read back in */
static void sh_print_profiling(application_profile *info, FILE *file) {
  size_t i, n, x, cur_interval, first_interval;
  arena_profile *aprof;
  arena_info *arena;
  per_event_profile_bw_info *profile_bw_aprof;
  
  /* If we're not printing every interval's profiling, just skip to the last
     interval. If we're not in SICM's runtime library, we're just going to 
     print all intervals no matter what. */
#ifdef SICM_RUNTIME
  if(profopts.print_profile_intervals) {
#endif
    first_interval = 0;
#ifdef SICM_RUNTIME
  } else {
    first_interval = info->num_intervals - 1;
  }
#endif

  for(cur_interval = first_interval; cur_interval < info->num_intervals; cur_interval++) {
    /* Common information for the whole application */
    fprintf(file, "===== BEGIN INTERVAL %zu PROFILING =====\n", cur_interval);
    fprintf(file, "Number of PROFILE_ALL events: %zu\n", info->num_profile_all_events);
    fprintf(file, "Number of PROFILE_BW events: %zu\n", info->num_profile_bw_events);
    fprintf(file, "Number of arenas: %zu\n", info->intervals[cur_interval].num_arenas);
    fprintf(file, "Upper Capacity: %zu\n", info->upper_capacity);
    fprintf(file, "Lower Capacity: %zu\n", info->lower_capacity);
    
/* Non-arena profiling info */
    if(info->has_profile_bw) {
      fprintf(file, "  BEGIN PROFILE_BW\n");
      for(n = 0; n < info->num_profile_bw_events; n++) {
        profile_bw_aprof = &(info->intervals[cur_interval].profile_bw.events[n]);
        fprintf(file, "    BEGIN EVENT %s\n", info->profile_bw_events[n]);
        fprintf(file, "      Total: %zu\n", profile_bw_aprof->total);
        fprintf(file, "      Current: %zu\n", profile_bw_aprof->current);
        fprintf(file, "      Peak: %zu\n", profile_bw_aprof->peak);
        fprintf(file, "    END EVENT %s\n", info->profile_bw_events[n]);
      }
      fprintf(file, "  END PROFILE_BW\n");
    }

    for(i = 0; i < info->intervals[cur_interval].num_arenas; i++) {
      aprof = info->intervals[cur_interval].arenas[i];
      if(!aprof) continue;

      /* Arena information and sites that are in this one arena */
      fprintf(file, "BEGIN ARENA %u\n", aprof->index);
      fprintf(file, "  Number of allocation sites: %d\n", aprof->num_alloc_sites);
      fprintf(file, "  Allocation sites: ");
      for(n = 0; n < aprof->num_alloc_sites; n++) {
        fprintf(file, "%d ", aprof->alloc_sites[n]);
      }
      fprintf(file, "\n");

      if(info->has_profile_all) {
        fprintf(file, "  BEGIN PROFILE_ALL\n");
        for(n = 0; n < info->num_profile_all_events; n++) {
          fprintf(file, "    BEGIN EVENT %s\n", info->profile_all_events[n]);
          fprintf(file, "      Total: %zu\n", aprof->profile_all.events[n].total);
          fprintf(file, "      Current: %zu\n", aprof->profile_all.events[n].current);
          fprintf(file, "      Peak: %zu\n", aprof->profile_all.events[n].peak);
          fprintf(file, "    END EVENT %s\n", info->profile_all_events[n]);
        }
        fprintf(file, "  END PROFILE_ALL\n");
      }
      if(info->has_profile_allocs) {
        fprintf(file, "  BEGIN PROFILE_ALLOCS\n");
        fprintf(file, "    Peak: %zu\n", aprof->profile_allocs.peak);
        fprintf(file, "    Current: %zu\n", aprof->profile_allocs.current);
        fprintf(file, "  END PROFILE_ALLOCS\n");
      }
      if(info->has_profile_rss) {
        fprintf(file, "  BEGIN PROFILE_RSS\n");
        fprintf(file, "    Peak: %zu\n", aprof->profile_rss.peak);
        fprintf(file, "    Current: %zu\n", aprof->profile_rss.current);
        fprintf(file, "  END PROFILE_RSS\n");
      }
      if(info->has_profile_extent_size) {
        fprintf(file, "  BEGIN PROFILE_EXTENT_SIZE\n");
        fprintf(file, "    Peak: %zu\n", aprof->profile_extent_size.peak);
        fprintf(file, "    Current: %zu\n", aprof->profile_extent_size.current);
        fprintf(file, "  END PROFILE_EXTENT_SIZE\n");
      }
      if(info->has_profile_online) {
        fprintf(file, "  BEGIN PROFILE_ONLINE\n");
        fprintf(file, "    Device: %d\n", aprof->profile_online.dev);
        fprintf(file, "    Hot: %d\n", aprof->profile_online.hot);
        fprintf(file, "    Hot Intervals: %zu\n",
                aprof->profile_online.num_hot_intervals);
        fprintf(file, "  END PROFILE_ONLINE\n");
      }
      fprintf(file, "END ARENA %u\n", aprof->index);
    }
    fprintf(file, "===== END INTERVAL PROFILING =====\n");
  }
}

/* Reads the above-printed information back into an application_profile struct. */
static application_profile *sh_parse_profiling(FILE *file) {
  /* Stores profiling information, returned */
  application_profile *ret;

  /* Used to read things in */
  ssize_t read;
  char *line, *tmpline;
  size_t len;
  char depth, profile_type;

  /* Temporaries */
  unsigned index, tmp_uint;
  size_t num_arenas, cur_arena_index, cur_event_index, cur_interval, tmp_sizet;
  int tmp_int, site;
  char *event;
  size_t i;
  arena_profile *cur_arena;
  per_event_profile_all_info *cur_event;
  per_event_profile_bw_info *profile_bw_cur_event;

  if(!file) {
    fprintf(stderr, "Invalid file pointer to be parsed. Aborting.\n");
    exit(1);
  }

  /* This keeps track of how deeply indented we are.
     0: Not in profiling information yet.
     1: In profiling information.
     2: In an arena.
     3: In a type of profiling.
     4: In a specific event.
  */
  depth = 0;

  /* Stores the type of profiling that we're currently looking at.
     0: profile_all
     1: profile_allocs
     2: profile_extent_size
     3: profile_rss
     4: profile_online
     5: profile_bw
  */
  profile_type = -1;

  ret = orig_calloc(1, sizeof(application_profile));
  ret->profile_all_events = NULL;
  ret->profile_bw_events = NULL;
  ret->num_intervals = 0;
  ret->intervals = NULL;
  len = 0;
  line = NULL;
  while(read = getline(&line, &len, file) != -1) {

    /* Here, we want to look for profiling information output */
    if(depth == 0) {
      if(strncmp(line, "===== BEGIN INTERVAL", 20) == 0) {
        depth = 1;
        num_arenas = 0;
        ret->num_intervals++;
        ret->intervals = realloc(ret->intervals, ret->num_intervals * sizeof(interval_profile));
        cur_interval = ret->num_intervals - 1;
        cur_arena = NULL;
        cur_event = NULL;
        profile_bw_cur_event = NULL;
        cur_arena_index = 0;
        cur_event_index = 0;
      }

    /* At this level, we're looking for three things:
       1. A number of arenas
       2. The start of a new arena
       3. The number of events of various types
       4. The end of an interval
       5. The start of some profile_bw information (since it's not per-arena)
    */
    } else if(depth == 1) {
      if(strncmp(line, "===== END INTERVAL\n", 18) == 0) {
        /* Up in depth */
        depth = 0;
        continue;
      } else if(sscanf(line, "Number of arenas: %zu", &num_arenas) == 1) {
        ret->intervals[cur_interval].num_arenas = num_arenas;
        ret->intervals[cur_interval].arenas = orig_calloc(num_arenas, sizeof(arena_profile *));
      } else if(sscanf(line, "Number of PROFILE_ALL events: %zu\n", &tmp_sizet) == 1) {
        ret->num_profile_all_events = tmp_sizet;
      } else if(sscanf(line, "Number of PROFILE_BW events: %zu\n", &tmp_sizet) == 1) {
        ret->num_profile_bw_events = tmp_sizet;
      } else if(sscanf(line, "Upper Capacity: %zu\n", &tmp_sizet) == 1) {
        ret->upper_capacity = tmp_sizet;
      } else if(sscanf(line, "Lower Capacity: %zu\n", &tmp_sizet) == 1) {
        ret->lower_capacity = tmp_sizet;
      } else if(strcmp(line, "  BEGIN PROFILE_BW\n") == 0) {
        /* Down in depth */
        depth = 2;
        profile_type = 5;
        ret->has_profile_bw = 1;
        if(!(ret->profile_bw_events)) {
          ret->profile_bw_events = orig_calloc(ret->num_profile_bw_events, sizeof(char *));
        }
        ret->intervals[cur_interval].profile_bw.events = orig_calloc(ret->num_profile_bw_events,
                                                                      sizeof(per_event_profile_bw_info));
        cur_event_index = 0;
        profile_bw_cur_event = &(ret->intervals[cur_interval].profile_bw.events[cur_event_index]);
      } else if(sscanf(line, "BEGIN ARENA %u", &index) == 1) {
        /* Down in depth */
        depth = 2;
        if(!num_arenas) {
          fprintf(stderr, "Didn't find a number of arenas. Aborting.\n");
          exit(1);
        }
        if((cur_arena_index > num_arenas - 1)) {
          fprintf(stderr, "Too many arenas (index %zu, max %zu) when parsing profiling info. Aborting.\n",
                  cur_arena_index, num_arenas - 1);
          exit(1);
        }
        cur_arena = orig_malloc(sizeof(arena_profile));
        ret->intervals[cur_interval].arenas[cur_arena_index] = cur_arena;
        cur_arena->index = index;
      } else {
        fprintf(stderr, "Didn't recognize a line in the profiling information at depth %d. Aborting.\n", depth);
        fprintf(stderr, "Line: %s\n", line);
        exit(1);
      }

    /* At this level, we're looking for:
       1. Per-arena information (numerous).
       2. The beginning of a type of profiling.
       3. The end of an arena.
       4. Some profile_bw information, if profile_type == 5.
    */
    } else if(depth == 2) {
      if(sscanf(line, "END ARENA %u\n", &tmp_uint) == 1) {
        /* Up in depth */
        depth = 1;
        cur_arena_index++;
      } else if(sscanf(line, "  Number of allocation sites: %d\n", &tmp_int)) {
        cur_arena->num_alloc_sites = tmp_int;
        cur_arena->alloc_sites = orig_malloc(tmp_int * sizeof(int));
      } else if(strncmp(line, "  Allocation sites: ", 20) == 0) {
        sscanf(line, "  Allocation sites: %n", &tmp_int);
        tmpline = line;
        tmpline = tmpline + tmp_int;
        i = 0;
        while(sscanf(tmpline, "%d %n", &site, &tmp_int) > 0) {
          if(i == cur_arena->num_alloc_sites) {
            fprintf(stderr, "There were too many allocation sites specified. Aborting.\n");
            exit(1);
          }
          cur_arena->alloc_sites[i] = site;
          tmpline += tmp_int;
          i++;
        }
      } else if(strcmp(line, "  BEGIN PROFILE_ALL\n") == 0) {
        /* Down in depth */
        depth = 3;
        profile_type = 0;
        ret->has_profile_all = 1;
        if(!(ret->profile_all_events)) {
          ret->profile_all_events = orig_calloc(ret->num_profile_all_events, sizeof(char *));
        }
        cur_arena->profile_all.events = orig_calloc(ret->num_profile_all_events,
                                                    sizeof(per_event_profile_all_info));
        cur_event_index = 0;
        cur_event = &(cur_arena->profile_all.events[cur_event_index]);
      } else if(strcmp(line, "  BEGIN PROFILE_ALLOCS\n") == 0) {
        depth = 3;
        profile_type = 1;
        ret->has_profile_allocs = 1;
      } else if(strcmp(line, "  BEGIN PROFILE_EXTENT_SIZE\n") == 0) {
        depth = 3;
        profile_type = 2;
        ret->has_profile_extent_size = 1;
      } else if(strcmp(line, "  BEGIN PROFILE_RSS\n") == 0) {
        depth = 3;
        profile_type = 3;
        ret->has_profile_rss = 1;
      } else if(strcmp(line, "  BEGIN PROFILE_ONLINE\n") == 0) {
        depth = 3;
        profile_type = 4;
        ret->has_profile_online = 1;
      } else if(profile_type == 5) {
        /* This is the case where we're in a PROFILE_BW block */
        if(strcmp(line, "  END PROFILE_BW\n") == 0) {
          /* Up in depth */
          depth = 1;
          profile_type = -1;
        } else if(sscanf(line, "    BEGIN EVENT %ms\n", &event) == 1) {
          /* Down in depth */
          if(cur_event_index > ret->num_profile_bw_events - 1) {
            fprintf(stderr, "Too many events specified. Aborting.\n");
            exit(1);
          }
          if(!(ret->profile_bw_events[cur_event_index])) {
            ret->profile_bw_events[cur_event_index] = orig_malloc((strlen(event) + 1) * sizeof(char));
            strcpy(ret->profile_bw_events[cur_event_index], event);
          }
          orig_free(event);
          profile_bw_cur_event = &(ret->intervals[cur_interval].profile_bw.events[cur_event_index]);
          depth = 3;
        } else {
          fprintf(stderr, "Didn't recognize a line in the profiling information at depth %d. Aborting.\n", depth);
          fprintf(stderr, "Line: %s\n", line);
          exit(1);
        }
      } else {
        fprintf(stderr, "Didn't recognize a line in the profiling information at depth %d. Aborting.\n", depth);
        fprintf(stderr, "Line: %s\n", line);
        exit(1);
      }
      
    /* Looking for information about a specific PROFILE_BW event. This
       is the same as PROFILE_ALL, but up one level of depth, since PROFILE_BW
       isn't per-arena. */
    } else if((depth == 3) && (profile_type == 5)) {
      if(sscanf(line, "      Total: %zu\n", &tmp_sizet)) {
        profile_bw_cur_event->total = tmp_sizet;
      } else if(sscanf(line, "      Peak: %zu\n", &tmp_sizet)) {
        profile_bw_cur_event->peak = tmp_sizet;
      } else if(sscanf(line, "      Current: %zu\n", &tmp_sizet)) {
        profile_bw_cur_event->current = tmp_sizet;
      } else if(sscanf(line, "    END EVENT %ms\n", &event) == 1) {
        /* Up in depth */
        orig_free(event);
        depth = 2;
        cur_event_index++;
      } else {
        fprintf(stderr, "Didn't recognize a line in the profiling information at depth %d. Aborting.\n", depth);
        fprintf(stderr, "Line: %s\n", line);
        exit(1);
      }

    /* Looking for:
       1. The start of a particular event
       2. The end of this profiling block
    */
    } else if((depth == 3) && (profile_type == 0)) {
      if(strcmp(line, "  END PROFILE_ALL\n") == 0) {
        /* Up in depth */
        depth = 2;
      } else if(sscanf(line, "    BEGIN EVENT %ms\n", &event) == 1) {
        /* Down in depth */
        if(cur_event_index > ret->num_profile_all_events - 1) {
          fprintf(stderr, "Too many events specified. Aborting.\n");
          exit(1);
        }
        if(!(ret->profile_all_events[cur_event_index])) {
          ret->profile_all_events[cur_event_index] = orig_malloc((strlen(event) + 1) * sizeof(char));
          strcpy(ret->profile_all_events[cur_event_index], event);
        }
        orig_free(event);
        cur_event = &(cur_arena->profile_all.events[cur_event_index]);
        depth = 4;
      } else {
        fprintf(stderr, "Didn't recognize a line in the profiling information at depth %d. Aborting.\n", depth);
        fprintf(stderr, "Line: %s\n", line);
        exit(1);
      }

    /* PROFILE_ALLOCS.
       Looking for:
       1. A peak.
       2. A current value.
       3. The end of this profiling block.
    */
    } else if((depth == 3) && (profile_type == 1)) {
      if(strcmp(line, "  END PROFILE_ALLOCS\n") == 0) {
        /* Up in depth */
        depth = 2;
      } else if(sscanf(line, "    Peak: %zu\n", &tmp_sizet)) {
        cur_arena->profile_allocs.peak = tmp_sizet;
      } else if(sscanf(line, "    Current: %zu\n", &tmp_sizet)) {
        cur_arena->profile_allocs.current = tmp_sizet;
      } else {
        fprintf(stderr, "Didn't recognize a line in the profiling information at depth %d. Aborting.\n", depth);
        fprintf(stderr, "Line: %s\n", line);
        exit(1);
      }

    /* PROFILE_EXTENT_SIZE.
       Looking for:
       1. A peak.
       2. A current value.
       3. The end of this profiling block.
    */
    } else if((depth == 3) && (profile_type == 2)) {
      if(strcmp(line, "  END PROFILE_EXTENT_SIZE\n") == 0) {
        /* Up in depth */
        depth = 2;
      } else if(sscanf(line, "    Peak: %zu\n", &tmp_sizet)) {
        cur_arena->profile_extent_size.peak = tmp_sizet;
      } else if(sscanf(line, "    Current: %zu\n", &tmp_sizet)) {
        cur_arena->profile_extent_size.current = tmp_sizet;
      } else {
        fprintf(stderr, "Didn't recognize a line in the profiling information at depth %d. Aborting.\n", depth);
        fprintf(stderr, "Line: %s\n", line);
        exit(1);
      }

    /* PROFILE_RSS.
       Looking for:
       1. A peak.
       2. A current value.
       3. The end of this profiling block.
    */
    } else if((depth == 3) && (profile_type == 3)) {
      if(strcmp(line, "  END PROFILE_RSS\n") == 0) {
        /* Up in depth */
        depth = 2;
      } else if(sscanf(line, "    Peak: %zu\n", &tmp_sizet)) {
        cur_arena->profile_rss.peak = tmp_sizet;
      } else if(sscanf(line, "    Current: %zu\n", &tmp_sizet)) {
        cur_arena->profile_rss.current = tmp_sizet;
      } else {
        fprintf(stderr, "Didn't recognize a line in the profiling information at depth %d. Aborting.\n", depth);
        fprintf(stderr, "Line: %s\n", line);
        exit(1);
      }

    /* PROFILE_ONLINE.
       Looking for:
       1. A device
       2. A binary hotness value.
       3. A number of hot intervals.
       4. The end of this profiling block.
    */
    } else if((depth == 3) && (profile_type == 4)) {
      if(strcmp(line, "  END PROFILE_ONLINE\n") == 0) {
        /* Up in depth */
        depth = 2;
      } else if(sscanf(line, "    Device: %d\n", &tmp_int)) {
        cur_arena->profile_online.dev = tmp_int;
      } else if(sscanf(line, "    Hot: %d\n", &tmp_int)) {
        cur_arena->profile_online.hot = tmp_int;
      } else if(sscanf(line, "    Hot Intervals: %zu\n", &tmp_sizet)) {
        cur_arena->profile_online.num_hot_intervals = tmp_sizet;
      } else {
        fprintf(stderr, "Didn't recognize a line in the profiling information at depth %d. Aborting.\n", depth);
        fprintf(stderr, "Line: %s\n", line);
        exit(1);
      }

    /* Looking for:
       1. A total
       2. A peak
       3. A current value.
       4. The end of this event block.
    */
    } else if((depth == 4) && (profile_type == 0)) {
      if(sscanf(line, "      Total: %zu\n", &tmp_sizet)) {
        cur_event->total = tmp_sizet;
      } else if(sscanf(line, "      Peak: %zu\n", &tmp_sizet)) {
        cur_event->peak = tmp_sizet;
      } else if(sscanf(line, "      Current: %zu\n", &tmp_sizet)) {
        cur_event->current = tmp_sizet;
      } else if(sscanf(line, "    END EVENT %ms\n", &event) == 1) {
        /* Up in depth */
        orig_free(event);
        depth = 3;
        cur_event_index++;
      } else {
        fprintf(stderr, "Didn't recognize a line in the profiling information at depth %d. Aborting.\n", depth);
        fprintf(stderr, "Line: %s\n", line);
        exit(1);
      }

    } else {
      fprintf(stderr, "Invalid depth (%d) detected in parsing. Aborting.\n", depth);
      exit(1);
    }
  }

  return ret;
}
