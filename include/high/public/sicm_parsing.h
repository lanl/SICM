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

#ifdef SICM_RUNTIME

/* Iterates over the structure and prints it out so that it can
 * be seamlessly read back in */
static void sh_print_profiling(application_profile *info, FILE *file) {
  size_t i, n, x;
  arena_profile *aprof;
  arena_info *arena;

  fprintf(file, "===== BEGIN SICM PROFILING INFORMATION =====\n");
  fprintf(file, "Number of PROFILE_ALL events: %zu\n", info->num_profile_all_events);
  fprintf(file, "Number of arenas: %zu\n", info->num_arenas);
  for(i = 0; i < info->num_arenas; i++) {
    aprof = info->arenas[i];

    /* Arena information and sites that are in this one arena */
    fprintf(file, "BEGIN ARENA %u\n", aprof->index);
    fprintf(file, "  Number of allocation sites: %d\n", aprof->num_alloc_sites);
    fprintf(file, "  Allocation sites: ");
    for(n = 0; n < aprof->num_alloc_sites; n++) {
      fprintf(file, "%d ", aprof->alloc_sites[n]);
    }
    fprintf(file, "\n");

    /* Interval information */
    fprintf(file, "  First interval: %zu\n", aprof->first_interval);
    fprintf(file, "  Number of intervals: %zu\n", aprof->num_intervals);

    if(profopts.should_profile_all) {
      fprintf(file, "  BEGIN PROFILE_ALL\n");
      for(n = 0; n < info->num_profile_all_events; n++) {
        fprintf(file, "    BEGIN EVENT %s\n", info->profile_all_events[n]);
        fprintf(file, "      Total: %zu\n", aprof->profile_all.events[n].total);
        fprintf(file, "      Peak: %zu\n", aprof->profile_all.events[n].peak);
        if(profopts.should_print_intervals) {
          fprintf(file, "      Intervals: ");
          for(x = 0; x < aprof->num_intervals; x++) {
            fprintf(file, "%zu ", aprof->profile_all.events[n].intervals[x]);
          }
          fprintf(file, "\n");
        }
        fprintf(file, "    END EVENT %s\n", profopts.profile_all_events[n]);
      }
      fprintf(file, "  END PROFILE_ALL\n");
    }
    if(profopts.should_profile_allocs) {
      fprintf(file, "  BEGIN PROFILE_ALLOCS\n");
      fprintf(file, "    Peak: %zu\n", aprof->profile_allocs.peak);
      if(profopts.should_print_intervals) {
        fprintf(file, "    Intervals: ");
        for(x = 0; x < aprof->num_intervals; x++) {
          fprintf(file, "%zu ", aprof->profile_allocs.intervals[x]);
        }
        fprintf(file, "\n");
      }
      fprintf(file, "  END PROFILE_ALLOCS\n");
    }
    if(profopts.should_profile_rss) {
      fprintf(file, "  BEGIN PROFILE_RSS\n");
      fprintf(file, "    Peak: %zu\n", aprof->profile_rss.peak);
      if(profopts.should_print_intervals) {
        fprintf(file, "    Intervals: ");
        for(x = 0; x < aprof->num_intervals; x++) {
          fprintf(file, "%zu ", aprof->profile_rss.intervals[x]);
        }
        fprintf(file, "\n");
      }
      fprintf(file, "  END PROFILE_RSS\n");
    }
    if(profopts.should_profile_extent_size) {
      fprintf(file, "  BEGIN PROFILE_EXTENT_SIZE\n");
      fprintf(file, "    Peak: %zu\n", aprof->profile_extent_size.peak);
      if(profopts.should_print_intervals) {
        fprintf(file, "    Intervals: ");
        for(x = 0; x < aprof->num_intervals; x++) {
          fprintf(file, "%zu ", aprof->profile_extent_size.intervals[x]);
        }
        fprintf(file, "\n");
      }
      fprintf(file, "  END PROFILE_EXTENT_SIZE\n");
    }
    fprintf(file, "END ARENA %u\n", aprof->index);

  }
  fprintf(file, "===== END SICM PROFILING INFORMATION =====\n");
}

#endif /* SICM_RUNTIME */

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
  size_t num_arenas, cur_arena_index, cur_event_index, tmp_sizet;
  int tmp_int, site;
  char *event;
  size_t i;
  arena_profile *cur_arena;
  per_event_profile_all_info *cur_event;

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
  */
  profile_type = -1;

  ret = orig_malloc(sizeof(application_profile));
  ret->profile_all_events = NULL;
  len = 0;
  line = NULL;
  cur_arena = NULL;
  cur_event = NULL;
  cur_arena_index = 0;
  cur_event_index = 0;
  while(read = getline(&line, &len, file) != -1) {

    /* Here, we want to look for profiling information output */
    if(depth == 0) {
      if(strcmp(line, "===== BEGIN SICM PROFILING INFORMATION =====\n") == 0) {
        depth = 1;
        num_arenas = 0;
      }

    /* At this level, we're looking for three things:
       1. A number of arenas
       2. The start of a new arena
       3. The number of events of various types
       3. The end of profiling information
    */
    } else if(depth == 1) {
      if(strcmp(line, "===== END SICM PROFILING INFORMATION =====\n") == 0) {
        /* Up in depth */
        depth = 0;
        break;
      } else if(sscanf(line, "Number of arenas: %zu", &num_arenas) == 1) {
        ret->num_arenas = num_arenas;
        ret->arenas = orig_calloc(num_arenas, sizeof(arena_profile *));
      } else if(sscanf(line, "Number of PROFILE_ALL events: %zu\n", &tmp_sizet) == 1) {
        ret->num_profile_all_events = tmp_sizet;
      } else if(sscanf(line, "BEGIN ARENA %u", &index) == 1) {
        /* Down in depth */
        depth = 2;
        if(!num_arenas) {
          fprintf(stderr, "Didn't find a number of arenas. Aborting.\n");
          exit(1);
        }
        if((cur_arena_index > num_arenas - 1)) {
          fprintf(stderr, "Too many arenas when parsing profiling info. Aborting.\n");
          exit(1);
        }
        cur_arena = orig_malloc(sizeof(arena_profile));
        ret->arenas[cur_arena_index] = cur_arena;
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
    */
    } else if(depth == 2) {
      if(sscanf(line, "END ARENA %u\n", &tmp_uint) == 1) {
        /* Up in depth */
        depth = 1;
        cur_arena_index++;
      } else if(sscanf(line, "  First interval: %zu", &tmp_sizet)) {
        cur_arena->first_interval = tmp_sizet;
      } else if(sscanf(line, "  Number of intervals: %zu", &tmp_sizet)) {
        cur_arena->num_intervals = tmp_sizet;
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
      } else if(strcmp(line, "  BEGIN PROFILE_EXTENT_SIZE\n") == 0) {
        depth = 3;
        profile_type = 2;
      } else if(strcmp(line, "  BEGIN PROFILE_RSS\n") == 0) {
        depth = 3;
        profile_type = 3;
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
       1. A peak
       2. An array of interval values
       3. The end of this profiling block
    */
    } else if((depth == 3) && (profile_type == 1)) {
      if(strcmp(line, "  END PROFILE_ALLOCS\n") == 0) {
        /* Up in depth */
        depth = 2;
      } else if(sscanf(line, "    Peak: %zu\n", &tmp_sizet)) {
        cur_arena->profile_allocs.peak = tmp_sizet;
      } else if(strncmp(line, "    Intervals: ", 17) == 0) {
        sscanf(line, "    Intervals: %n", &tmp_int);
        tmpline = line;
        tmpline = tmpline + tmp_int;
        i = 0;
        while(sscanf(tmpline, "%zu %n", &tmp_sizet, &tmp_int) > 0) {
          if(i == cur_arena->num_intervals) {
            fprintf(stderr, "There were too many intervals specified. Aborting.\n");
            exit(1);
          }
          cur_arena->profile_allocs.intervals[i] = tmp_sizet;
          tmpline += tmp_int;
          i++;
        }
      } else {
        fprintf(stderr, "Didn't recognize a line in the profiling information at depth %d. Aborting.\n", depth);
        fprintf(stderr, "Line: %s\n", line);
        exit(1);
      }

    /* PROFILE_EXTENT_SIZE.
       Looking for:
       1. A peak
       2. An array of interval values
       3. The end of this profiling block
    */
    } else if((depth == 3) && (profile_type == 2)) {
      if(strcmp(line, "  END PROFILE_EXTENT_SIZE\n") == 0) {
        /* Up in depth */
        depth = 2;
      } else if(sscanf(line, "    Peak: %zu\n", &tmp_sizet)) {
        cur_arena->profile_extent_size.peak = tmp_sizet;
      } else if(strncmp(line, "    Intervals: ", 17) == 0) {
        sscanf(line, "    Intervals: %n", &tmp_int);
        tmpline = line;
        tmpline = tmpline + tmp_int;
        i = 0;
        while(sscanf(tmpline, "%zu %n", &tmp_sizet, &tmp_int) > 0) {
          if(i == cur_arena->num_intervals) {
            fprintf(stderr, "There were too many intervals specified. Aborting.\n");
            exit(1);
          }
          cur_arena->profile_extent_size.intervals[i] = tmp_sizet;
          tmpline += tmp_int;
          i++;
        }
      } else {
        fprintf(stderr, "Didn't recognize a line in the profiling information at depth %d. Aborting.\n", depth);
        fprintf(stderr, "Line: %s\n", line);
        exit(1);
      }

    /* PROFILE_RSS.
       Looking for:
       1. A peak
       2. An array of interval values
       3. The end of this profiling block
    */
    } else if((depth == 3) && (profile_type == 3)) {
      if(strcmp(line, "  END PROFILE_RSS\n") == 0) {
        /* Up in depth */
        depth = 2;
      } else if(sscanf(line, "    Peak: %zu\n", &tmp_sizet)) {
        cur_arena->profile_rss.peak = tmp_sizet;
      } else if(strncmp(line, "    Intervals: ", 17) == 0) {
        sscanf(line, "    Intervals: %n", &tmp_int);
        tmpline = line;
        tmpline = tmpline + tmp_int;
        i = 0;
        while(sscanf(tmpline, "%zu %n", &tmp_sizet, &tmp_int) > 0) {
          if(i == cur_arena->num_intervals) {
            fprintf(stderr, "There were too many intervals specified. Aborting.\n");
            exit(1);
          }
          cur_arena->profile_rss.intervals[i] = tmp_sizet;
          tmpline += tmp_int;
          i++;
        }
      } else {
        fprintf(stderr, "Didn't recognize a line in the profiling information at depth %d. Aborting.\n", depth);
        fprintf(stderr, "Line: %s\n", line);
        exit(1);
      }

    /* Looking for:
       1. A total
       2. A peak
       3. An array of interval values
       4. The end of this event block
    */
    } else if((depth == 4) && (profile_type == 0)) {
      if(sscanf(line, "      Total: %zu\n", &tmp_sizet)) {
        cur_event->total = tmp_sizet;
      } else if(sscanf(line, "      Peak: %zu\n", &tmp_sizet)) {
        cur_event->peak = tmp_sizet;
      } else if(strncmp(line, "      Intervals: ", 17) == 0) {
        sscanf(line, "      Intervals: %n", &tmp_int);
        tmpline = line;
        tmpline = tmpline + tmp_int;
        i = 0;
        while(sscanf(tmpline, "%zu %n", &tmp_sizet, &tmp_int) > 0) {
          if(i == cur_arena->num_intervals) {
            fprintf(stderr, "There were too many intervals specified. Aborting.\n");
            exit(1);
          }
          cur_event->intervals[i] = tmp_sizet;
          tmpline += tmp_int;
          i++;
        }
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
