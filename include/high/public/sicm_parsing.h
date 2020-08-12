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
  profile_rss_info *profile_rss_aprof;
  per_skt_profile_bw_info *profile_bw_aprof;
  per_skt_profile_latency_info *profile_latency_aprof;
  
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
    if(info->has_profile_all) {
      fprintf(file, "Number of PROFILE_ALL events: %zu\n", info->num_profile_all_events);
    }
    if(info->has_profile_bw) {
      fprintf(file, "Number of PROFILE_BW sockets: %zu\n", info->num_profile_skts);
    }
    fprintf(file, "Time: %.4lf\n", info->intervals[cur_interval].time);
    if(info->has_profile_online) {
      fprintf(file, "Reconfigure: %d\n", info->intervals[cur_interval].profile_online.reconfigure);
      fprintf(file, "Phase Change: %d\n", info->intervals[cur_interval].profile_online.phase_change);
      fprintf(file, "Upper Capacity: %zu\n", info->upper_capacity);
      fprintf(file, "Lower Capacity: %zu\n", info->lower_capacity);
    }
    fprintf(file, "Number of arenas: %zu\n", info->intervals[cur_interval].num_arenas);
    fprintf(file, "Maximum index: %zu\n", info->intervals[cur_interval].max_index);
    
    /* Non-arena profiling info */
    if(info->has_profile_bw) {
      fprintf(file, "  BEGIN PROFILE_BW\n");
      for(n = 0; n < info->num_profile_skts; n++) {
        profile_bw_aprof = &(info->intervals[cur_interval].profile_bw.skt[n]);
        fprintf(file, "    BEGIN SOCKET %d\n", info->profile_skts[n]);
        fprintf(file, "      Current: %zu\n", profile_bw_aprof->current);
        fprintf(file, "      Peak: %zu\n", profile_bw_aprof->peak);
        fprintf(file, "    END SOCKET %d\n", info->profile_skts[n]);
      }
      fprintf(file, "  END PROFILE_BW\n");
    }
    if(info->has_profile_rss) {
      fprintf(file, "  BEGIN PROFILE_RSS\n");
      profile_rss_aprof = &(info->intervals[cur_interval].profile_rss);
      fprintf(file, "    Time: %f\n", profile_rss_aprof->time);
      fprintf(file, "  END PROFILE_RSS\n");
    }
    if(info->has_profile_latency) {
      fprintf(file, "  BEGIN PROFILE_LATENCY\n");
      for(n = 0; n < info->num_profile_skts; n++) {
        profile_latency_aprof = &(info->intervals[cur_interval].profile_latency.skt[n]);
        fprintf(file, "    BEGIN SOCKET %d\n", info->profile_skts[n]);
        fprintf(file, "      Upper Read Current: %f\n", profile_latency_aprof->upper_read_current);
        fprintf(file, "      Upper Read Peak: %f\n", profile_latency_aprof->upper_read_peak);
        fprintf(file, "      Upper Write Current: %f\n", profile_latency_aprof->upper_write_current);
        fprintf(file, "      Upper Write Peak: %f\n", profile_latency_aprof->upper_write_peak);
        fprintf(file, "      Lower Read Current: %f\n", profile_latency_aprof->lower_read_current);
        fprintf(file, "      Lower Read Peak: %f\n", profile_latency_aprof->lower_read_peak);
        fprintf(file, "      Lower Write Current: %f\n", profile_latency_aprof->lower_write_current);
        fprintf(file, "      Lower Write Peak: %f\n", profile_latency_aprof->lower_write_peak);
        fprintf(file, "      Read Ratio: %f\n", profile_latency_aprof->read_ratio);
        fprintf(file, "      Write Ratio: %f\n", profile_latency_aprof->write_ratio);
        fprintf(file, "      Read Ratio CMA: %f\n", profile_latency_aprof->read_ratio_cma);
        fprintf(file, "      Write Ratio CMA: %f\n", profile_latency_aprof->write_ratio_cma);
        fprintf(file, "    END SOCKET %d\n", info->profile_skts[n]);
      }
      fprintf(file, "  END PROFILE_LATENCY\n");
    }

    for(i = 0; i < info->intervals[cur_interval].max_index; i++) {
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
      if(info->has_profile_bw_relative) {
        fprintf(file, "  BEGIN PROFILE_BW_RELATIVE\n");
        fprintf(file, "    Total: %zu\n", aprof->profile_bw.total);
        fprintf(file, "    Current: %zu\n", aprof->profile_bw.current);
        fprintf(file, "    Peak: %zu\n", aprof->profile_bw.peak);
        fprintf(file, "  END PROFILE_BW_RELATIVE\n");
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
  size_t num_arenas,
         max_index,
         cur_arena_index,
         cur_event_index,
         cur_skt_index,
         cur_interval,
         tmp_sizet;
  double tmp_double;
  char tmp_char;
  int tmp_int, site;
  char *event;
  size_t i;
  arena_profile *cur_arena;
  per_event_profile_all_info *cur_event;
  per_skt_profile_bw_info *profile_bw_cur_skt;
  per_skt_profile_latency_info *profile_latency_cur_skt;

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
     6: profile_bw_relative
     7: profile_latency
  */
  profile_type = -1;

  ret = orig_calloc(1, sizeof(application_profile));
  ret->profile_all_events = NULL;
  ret->profile_skts = NULL;
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
        profile_bw_cur_skt = NULL;
        cur_arena_index = 0;
        cur_event_index = 0;
        cur_skt_index = 0;
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
        /* ret->intervals[cur_interval].arenas = orig_calloc(num_arenas, sizeof(arena_profile *)); */
      } else if(sscanf(line, "Maximum index: %zu", &max_index) == 1) {
        ret->intervals[cur_interval].max_index = max_index;
        ret->intervals[cur_interval].arenas = orig_calloc(max_index, sizeof(arena_profile *));
      } else if(sscanf(line, "Number of PROFILE_ALL events: %zu\n", &tmp_sizet) == 1) {
        ret->num_profile_all_events = tmp_sizet;
      } else if(sscanf(line, "Number of PROFILE_BW sockets: %zu\n", &tmp_sizet) == 1) {
        ret->num_profile_skts = tmp_sizet;
      } else if(sscanf(line, "Upper Capacity: %zu\n", &tmp_sizet) == 1) {
        ret->upper_capacity = tmp_sizet;
      } else if(sscanf(line, "Lower Capacity: %zu\n", &tmp_sizet) == 1) {
        ret->lower_capacity = tmp_sizet;
      } else if(sscanf(line, "Time: %lf\n", &tmp_double) == 1) {
        ret->intervals[cur_interval].time = tmp_double;
      } else if(sscanf(line, "Reconfigure: %d\n", &tmp_char) == 1) {
        ret->intervals[cur_interval].profile_online.reconfigure = tmp_char;
      } else if(sscanf(line, "Phase Change: %d\n", &tmp_char) == 1) {
        ret->intervals[cur_interval].profile_online.phase_change = tmp_char;
      } else if(strcmp(line, "  BEGIN PROFILE_BW\n") == 0) {
        /* Down in depth */
        depth = 2;
        profile_type = 5;
        ret->has_profile_bw = 1;
        if(!(ret->profile_skts)) {
          ret->profile_skts = orig_calloc(ret->num_profile_skts, sizeof(int));
        }
        ret->intervals[cur_interval].profile_bw.skt = orig_calloc(ret->num_profile_skts,
                                                                   sizeof(per_skt_profile_bw_info));
        cur_skt_index = 0;
        profile_bw_cur_skt = &(ret->intervals[cur_interval].profile_bw.skt[cur_skt_index]);
      } else if(strcmp(line, "  BEGIN PROFILE_RSS\n") == 0) {
        /* Down in depth */
        depth = 2;
        profile_type = 3;
        ret->has_profile_rss = 1;
      } else if(strcmp(line, "  BEGIN PROFILE_LATENCY\n") == 0) {
        /* Down in depth */
        depth = 2;
        profile_type = 7;
        ret->has_profile_latency = 1;
        if(!(ret->profile_skts)) {
          ret->profile_skts = orig_calloc(ret->num_profile_skts, sizeof(int));
        }
        ret->intervals[cur_interval].profile_latency.skt = orig_calloc(ret->num_profile_skts,
                                                                   sizeof(per_skt_profile_latency_info));
        cur_skt_index = 0;
        profile_latency_cur_skt = &(ret->intervals[cur_interval].profile_latency.skt[cur_skt_index]);
      } else if(sscanf(line, "BEGIN ARENA %u", &index) == 1) {
        /* Down in depth */
        depth = 2;
        if(!max_index) {
          fprintf(stderr, "Didn't find a maximum index. Aborting.\n");
          exit(1);
        }
        if((cur_arena_index > max_index - 1)) {
          fprintf(stderr, "Too many arenas (index %zu, max %zu) when parsing profiling info. Aborting.\n",
                  cur_arena_index, max_index);
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
      } else if(strcmp(line, "  BEGIN PROFILE_BW_RELATIVE\n") == 0) {
        depth = 3;
        profile_type = 6;
        ret->has_profile_bw_relative = 1;
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
        } else if(sscanf(line, "    BEGIN SOCKET %d\n", &tmp_int) == 1) {
          /* Down in depth */
          if(cur_skt_index > ret->num_profile_skts - 1) {
            fprintf(stderr, "Too many sockets specified. Aborting.\n");
            exit(1);
          }
          ret->profile_skts[cur_skt_index] = tmp_int;
          profile_bw_cur_skt = &(ret->intervals[cur_interval].profile_bw.skt[cur_skt_index]);
          depth = 3;
        } else {
          fprintf(stderr, "Didn't recognize a line in the profiling information at depth %d. Aborting.\n", depth);
          fprintf(stderr, "Line: %s\n", line);
          exit(1);
        }
      } else if(profile_type == 3) {
        /* This is the case where we're in a PROFILE_RSS block */
        if(strcmp(line, "  END PROFILE_RSS\n") == 0) {
          /* Up in depth */
          depth = 1;
          profile_type = -1;
        } else if(sscanf(line, "    Time: %lf\n", &tmp_double) == 1) {
          ret->intervals[cur_interval].profile_rss.time = tmp_double;
        } else {
          fprintf(stderr, "Didn't recognize a line in the profiling information at depth %d. Aborting.\n", depth);
          fprintf(stderr, "Line: %s\n", line);
          exit(1);
        }
      } else if(profile_type == 7) {
        /* This is the case where we're in a PROFILE_LATENCY block */
        if(strcmp(line, "  END PROFILE_LATENCY\n") == 0) {
          /* Up in depth */
          depth = 1;
          profile_type = -1;
        } else if(sscanf(line, "    BEGIN SOCKET %d\n", &tmp_int) == 1) {
          /* Down in depth */
          if(cur_skt_index > ret->num_profile_skts - 1) {
            fprintf(stderr, "Too many sockets specified. Aborting.\n");
            exit(1);
          }
          ret->profile_skts[cur_skt_index] = tmp_int;
          profile_latency_cur_skt = &(ret->intervals[cur_interval].profile_latency.skt[cur_skt_index]);
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
      if(sscanf(line, "      Peak: %zu\n", &tmp_sizet)) {
        profile_bw_cur_skt->peak = tmp_sizet;
      } else if(sscanf(line, "      Current: %zu\n", &tmp_sizet)) {
        profile_bw_cur_skt->current = tmp_sizet;
      } else if(sscanf(line, "    END SOCKET %d\n", &tmp_int) == 1) {
        /* Up in depth */
        depth = 2;
        cur_skt_index++;
      } else {
        fprintf(stderr, "Didn't recognize a line in the profiling information at depth %d. Aborting.\n", depth);
        fprintf(stderr, "Line: %s\n", line);
        exit(1);
      }
      
    /* Looking for information about a specific PROFILE_LATENCY event. This
       is the same as PROFILE_ALL, but up one level of depth, since PROFILE_LATENCY
       isn't per-arena. */
    } else if((depth == 3) && (profile_type == 7)) {
      if(sscanf(line, "      Upper Read Peak: %lf\n", &tmp_double)) {
        profile_latency_cur_skt->upper_read_peak = tmp_double;
      } else if(sscanf(line, "      Upper Read Current: %lf\n", &tmp_double)) {
        profile_latency_cur_skt->upper_read_current = tmp_double;
      } else if(sscanf(line, "      Upper Write Peak: %lf\n", &tmp_double)) {
        profile_latency_cur_skt->upper_write_peak = tmp_double;
      } else if(sscanf(line, "      Upper Write Current: %lf\n", &tmp_double)) {
        profile_latency_cur_skt->upper_write_current = tmp_double;
      } else if(sscanf(line, "      Lower Read Peak: %lf\n", &tmp_double)) {
        profile_latency_cur_skt->lower_read_peak = tmp_double;
      } else if(sscanf(line, "      Lower Read Current: %lf\n", &tmp_double)) {
        profile_latency_cur_skt->lower_read_current = tmp_double;
      } else if(sscanf(line, "      Lower Write Peak: %lf\n", &tmp_double)) {
        profile_latency_cur_skt->lower_write_peak = tmp_double;
      } else if(sscanf(line, "      Lower Write Current: %lf\n", &tmp_double)) {
        profile_latency_cur_skt->lower_write_current = tmp_double;
      } else if(sscanf(line, "      Read Ratio: %lf\n", &tmp_double)) {
        profile_latency_cur_skt->read_ratio = tmp_double;
      } else if(sscanf(line, "      Write Ratio: %lf\n", &tmp_double)) {
        profile_latency_cur_skt->write_ratio = tmp_double;
      } else if(sscanf(line, "      Read Ratio CMA: %lf\n", &tmp_double)) {
        profile_latency_cur_skt->read_ratio_cma = tmp_double;
      } else if(sscanf(line, "      Write Ratio CMA: %lf\n", &tmp_double)) {
        profile_latency_cur_skt->write_ratio_cma = tmp_double;
      } else if(sscanf(line, "    END SOCKET %d\n", &tmp_int) == 1) {
        /* Up in depth */
        depth = 2;
        cur_skt_index++;
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
      
    /* PROFILE_BW_RELATIVE
       Looking for:
       1. A peak.
       2. A current value.
       3. A total.
       4. The end of this profiling block.
    */
    } else if((depth == 3) && (profile_type == 6)) {
      if(strcmp(line, "  END PROFILE_BW_RELATIVE\n") == 0) {
        /* Up in depth */
        depth = 2;
      } else if(sscanf(line, "    Peak: %zu\n", &tmp_sizet)) {
        cur_arena->profile_bw.peak = tmp_sizet;
      } else if(sscanf(line, "    Current: %zu\n", &tmp_sizet)) {
        cur_arena->profile_bw.current = tmp_sizet;
      } else if(sscanf(line, "    Total: %zu\n", &tmp_sizet)) {
        cur_arena->profile_bw.total = tmp_sizet;
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
