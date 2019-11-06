/*
 * Contains code for parsing output of the high-level interface.
 * Also outputs profiling information to a file.
 */

#include <inttypes.h>
#include <limits.h>
#include <stdio.h>
#include "sicm_profile.h"
#include "sicm_tree.h"

/* Iterates over the structure and prints it out so that it can
 * be seamlessly read back in */
void sh_print_profiling(profile_info **info) {
  size_t i, n, x, num_arenas;
  profile_info *profinfo;
  arena_info *arena;

  /* Figure out how many valid arenas there are. */
  num_arenas = 0;
  arena_arr_for(i) {
    prof_check_good(arena, profinfo, i);
    num_arenas++;
  }

  printf("===== BEGIN SICM PROFILING INFORMATION =====\n");
  printf("Number of PROFILE_ALL events: %zu\n", profopts.num_profile_all_events);
  printf("Number of arenas: %zu\n", num_arenas);
  arena_arr_for(i) {
    prof_check_good(arena, profinfo, i);

    /* Arena information and sites that are in this one arena */
    printf("BEGIN ARENA %u\n", arena->index);
    printf("  Number of allocation sites: %d\n", arena->num_alloc_sites);
    printf("  Allocation sites: ");
    for(n = 0; n < arena->num_alloc_sites; n++) {
      printf("%d ", arena->alloc_sites[n]);
    }
    printf("\n");

    /* Interval information */
    printf("  First interval: %zu\n", profinfo->first_interval);
    printf("  Number of intervals: %zu\n", profinfo->num_intervals);

    if(profopts.should_profile_all) {
      printf("  BEGIN PROFILE_ALL\n");
      for(n = 0; n < profopts.num_profile_all_events; n++) {
        printf("    BEGIN EVENT %s\n", profopts.profile_all_events[n]);
        printf("      Total: %zu\n", profinfo->profile_all.events[n].total);
        printf("      Peak: %zu\n", profinfo->profile_all.events[n].peak);
        if(profopts.should_print_intervals) {
          printf("      Intervals: ");
          for(x = 0; x < profinfo->num_intervals; x++) {
            printf("%zu ", profinfo->profile_all.events[n].intervals[x]);
          }
          printf("\n");
        }
        printf("    END EVENT %s\n", profopts.profile_all_events[n]);
      }
      printf("  END PROFILE_ALL\n");
    }
    printf("END ARENA %u\n", arena->index);

  }
  printf("===== END SICM PROFILING INFORMATION =====\n");
}

/* Reads the above-printed information back into an array of prev_profile_info structs. */
prev_app_info *sh_parse_profiling(FILE *file) {
  /* Stores all profiling information */
  prev_app_info *ret;

  /* Used to read things in */
  ssize_t read;
  char *line;
  size_t len;
  char depth, profile_type;

  /* Temporaries */
  unsigned index, tmp_uint;
  size_t num_arenas, cur_arena_index, cur_event_index, tmp_sizet;
  int tmp_int, site;
  char *event;
  size_t i;
  prev_profile_info *cur_arena;
  per_event_profile_all_info *cur_event;

  if(!file) {
    fprintf(stderr, "Invalid file pointer to be parsed. Aborting.\n");
    exit(1);
  }

  printf("Parsing profiling information.\n");

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
  */
  profile_type = -1;

  ret = malloc(sizeof(prev_app_info));
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
        printf("Found the beginning of profiling info\n");
        depth = 1;
        num_arenas = 0;
        continue;
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
        ret->prev_info_arr = calloc(num_arenas, sizeof(prev_profile_info));
        continue;
      } else if(sscanf(line, "Number of PROFILE_ALL events: %zu\n", &tmp_sizet) == 1) {
        ret->num_profile_all_events = tmp_sizet;
        continue;
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
        cur_arena = &(ret->prev_info_arr[cur_arena_index]);
        cur_arena->index = index;
        continue;
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
        continue;
      } else if(sscanf(line, "  First interval: %zu", &tmp_sizet)) {
        cur_arena->info.first_interval = tmp_sizet;
        continue;
      } else if(sscanf(line, "  Number of intervals: %zu", &tmp_sizet)) {
        cur_arena->info.num_intervals = tmp_sizet;
        continue;
      } else if(sscanf(line, "  Number of allocation sites: %d\n", &tmp_int)) {
        cur_arena->num_alloc_sites = tmp_int;
        cur_arena->alloc_sites = malloc(tmp_int * sizeof(int));
        continue;
      } else if(strncmp(line, "  Allocation sites: ", 20) == 0) {
        sscanf(line, "  Allocation sites: %n", &tmp_int);
        line = line + tmp_int;
        i = 0;
        while(sscanf(line, "%d %n", &site, &tmp_int) > 0) {
          if(i == cur_arena->num_alloc_sites) {
            fprintf(stderr, "There were too many allocation sites specified. Aborting.\n");
            exit(1);
          }
          cur_arena->alloc_sites[i] = site;
          line += tmp_int;
          i++;
        }
        continue;
      } else if(strcmp(line, "  BEGIN PROFILE_ALL\n") == 0) {
        /* Down in depth */
        depth = 3;
        profile_type = 0;
        if(!(ret->profile_all_events)) {
          ret->profile_all_events = calloc(ret->num_profile_all_events, sizeof(char *));
        }
        cur_arena->info.profile_all.events = calloc(ret->num_profile_all_events,
                                                    sizeof(per_event_profile_all_info));
        cur_event_index = 0;
        cur_event = &(cur_arena->info.profile_all.events[cur_event_index]);
        continue;
      } else if(strcmp(line, "  BEGIN PROFILE_ALLOCS\n") == 0) {
        depth = 3;
        profile_type = 1;
        continue;
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
        continue;
      } else if(sscanf(line, "    BEGIN EVENT %ms\n", &event) == 1) {
        /* Down in depth */
        if(cur_event_index > ret->num_profile_all_events - 1) {
          fprintf(stderr, "Too many events specified. Aborting.\n");
          exit(1);
        }
        if(!(ret->profile_all_events[cur_event_index])) {
          ret->profile_all_events[cur_event_index] = malloc((strlen(event) + 1) * sizeof(char));
          strcpy(ret->profile_all_events[cur_event_index], event);
        }
        free(event);
        cur_event = &(cur_arena->info.profile_all.events[cur_event_index]);
        depth = 4;
        continue;
      } else {
        fprintf(stderr, "Didn't recognize a line in the profiling information at depth %d. Aborting.\n", depth);
        fprintf(stderr, "Line: %s\n", line);
        exit(1);
      }

    /* Looking for:
       1. A peak
       2. An array of interval values
       3. The end of this profiling block
    */
    } else if((depth == 3) && (profile_type == 1)) {
      if(strcmp(line, "  END PROFILE_ALLOCS\n") == 0) {
        /* Up in depth */
        depth = 2;
        continue;
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
        line = line + tmp_int;
        i = 0;
        while(sscanf(line, "%zu %n", &tmp_sizet, &tmp_int) > 0) {
          if(i == cur_arena->info.num_intervals) {
            fprintf(stderr, "There were too many intervals specified. Aborting.\n");
            exit(1);
          }
          cur_event->intervals[i] = tmp_sizet;
          line += tmp_int;
          i++;
        }
        continue;
      } else if(sscanf(line, "    END EVENT %ms\n", &event) == 1) {
        /* Up in depth */
        free(event);
        depth = 3;
        cur_event_index++;
        continue;
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
}
