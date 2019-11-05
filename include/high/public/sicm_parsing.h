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
          printf("        ");
          for(x = 0; x < profinfo->num_intervals; x++) {
            printf("%zu ", profinfo->profile_all.events[n].intervals[x]);
          }
          printf("\n");
        }
        printf("    END EVENT\n", profopts.profile_all_events[n]);
      }
      printf("  END PROFILE_ALL\n");
    }
    printf("END ARENA\n", arena->index);

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
  unsigned index;
  size_t num_arenas, cur_arena_index, tmp_sizet;
  int tmp_int, site;

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
  */
  profile_type = -1;

  ret = malloc(sizeof(prev_app_info));
  len = 0;
  line = NULL;
  cur_arena_index = 0;
  while(read = getline(&line, &len, file) != -1) {

    /* Here, we want to look for profiling information output */
    if(depth == 0) {
      if(strcmp(line, "===== BEGIN SICM PROFILING INFORMATION =====\n") == 0) {
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
      } else if(sscanf("Number of PROFILE_ALL events: %zu\n", &tmp_sizet) == 1) {
        ret->num_profile_all_events = tmp_sizet;
        continue;
      } else if(sscanf(line, "BEGIN ARENA %u", index) == 1) {
        /* Down in depth */
        depth = 2;
        if(!num_arenas) {
          fprintf(stderr, "Didn't find a number of arenas. Aborting.\n");
          exit(1);
        }
        if((cur_arena_index == num_arenas - 1) {
          fprintf(stderr, "Too many arenas when parsing profiling info. Aborting.\n");
          exit(1);
        }
        ret->prev_info_arr[cur_arena_index].index = index;
        continue;
      } else {
        fprintf(stderr, "Didn't recognize a line in the profiling information. Aborting.\n");
        exit(1);
      }

    /* At this level, we're looking for:
       1. Per-arena information (numerous).
       2. The beginning of a type of profiling.
       3. The end of an arena.
    */
    } else if(depth == 2) {
      if(strcmp(line, "END ARENA") == 0) {
        /* Up in depth */
        depth = 1;
        continue;
      } else if(sscanf(line, "First interval: %zu", &tmp_sizet)) {
        ret->first_interval = tmp_sizet;
        continue;
      } else if(sscanf(line, "Number of intervals: %zu", &tmp_sizet)) {
        ret->intervals = tmp_sizet;
        continue;
      } else if(sscanf(line, "Number of allocation sites: %d", &tmp_int)) {
        ret->prev_info_arr[cur_arena_index].num_alloc_sites = tmp_int;
        ret->prev_info_arr[cur_arena_index].alloc_sites = malloc(tmp_int * sizeof(int));
        continue;
      } else if(strcmp(line, "Allocation sites:") == 0) {
        sscanf(line, "Allocation sites: %n", &tmp_int);
        line = line + tmp_int;
        while(sscanf(line, "%d %n", &site, &tmp_int) > 0) {
          printf("Found site %d\n", site);
          ret->prev_info_arr[cur_arena_index].alloc_sites[i] = site;
        }
      } else if(strcmp(line, "BEGIN PROFILE_ALL") == 0) {
        /* Down in depth */
        depth = 3;
        profile_type = 0;
        continue;
      } else {
        fprintf(stderr, "Didn't recognize a line in the profiling information. Aborting.\n");
        exit(1);
      }

    /* Looking for:
       1. The start of a particular event
       2. The end of this profiling block
    */
    } else if((depth == 3) && (profile_type == 0)) {
      if(strcmp(line, "END PROFILE_ALL") == 0) {
        /* Up in depth */
        depth = 2;
        continue;
      } else if(sscanf(line, "BEGIN EVENT %s", event)) {
      } else {
        fprintf(stderr, "Didn't recognize a line in the profiling information. Aborting.\n");
        exit(1);
      }
    }
  }
}
