/*******************
  The `ski` online strategy.
*******************/

/* Returns 1 if the bandwidth tanked since before the last rebinding */
char bw_tanked() {
  if(get_online_data()->prev_bw &&
     get_online_data()->cur_bw &&
     ((((double) get_online_data()->cur_bw) / get_online_data()->prev_bw) < 0.9)) {
    return 1;
  }
  return 0;
}

/* Update prev_bw and cur_bw */
void update_bw() {
  size_t i;
  
  get_online_data()->prev_bw = get_online_data()->cur_bw;
  get_online_data()->cur_bw = 0;
  for(i = 0; i < profopts.num_profile_skt_cpus; i++) {
    get_online_data()->cur_bw += get_bw_skt_prof(i)->current;
  }
  
  if(profopts.profile_online_debug_file) {
    fprintf(profopts.profile_online_debug_file,
      "Bandwidth: %zu->%zu\n", get_online_data()->prev_bw, get_online_data()->cur_bw);
    fflush(profopts.profile_online_debug_file);
  }
}

/* This is the amount of time, in milliseconds, that it will
    take to do the complete rebinding. */
static inline size_t penalty_move(int index) {
  size_t retval, cap;
  
  cap = get_objmap_arena_prof(index)->current_present_bytes;
  retval = ((double) cap)
           / 2097152; /* 2 GB/s, converted to bytes/ms */
  if(retval < 50) {
    retval = 50;
  }
  return retval;
}

/* This is the amount of time in milliseconds that we've wasted thus
   far by keeping `accesses` number of accesses in the AEP */
static inline size_t penalty_stay(int index) {
  size_t retval, accesses;
  
  /* This is the number of "bandwidth accesses" that the site has gotten on
     DRAM over the whole run. The unit coming from profile_bw is in
     cache lines. */
  accesses = get_bw_event_prof(index, 1)->total_count;
  retval =  (accesses)
            * 0.0003; /* We lose 300 nanoseconds per access, to ms */
  if(profopts.profile_online_debug_file) {
    fprintf(profopts.profile_online_debug_file,
            "dram: %zu, aep: %zu, penalty_stay: %zu.\n",
            get_bw_event_prof(index, 0)->total_count,
            get_bw_event_prof(index, 1)->total_count,
            retval);
    fflush(profopts.profile_online_debug_file);
  }
  return retval;
}

/* This is the amount of time in milliseconds that we *would* waste by
   displacing this site in the upper tier (that is, moving it from the upper
   tier to the lower tier) */
static inline size_t penalty_displace(int index) {
  size_t retval, accesses;
  
  accesses = get_bw_event_prof(index, 0)->total_count;
  retval =  (accesses)
            * 0.0003; /* We lose 300 nanoseconds per access, to ms */
  if(profopts.profile_online_debug_file) {
    fprintf(profopts.profile_online_debug_file,
            "dram: %zu, aep: %zu, penalty_displace: %zu.\n",
            get_bw_event_prof(index, 0)->total_count,
            get_bw_event_prof(index, 1)->total_count,
            retval);
    fflush(profopts.profile_online_debug_file);
  }
  return retval;
}

/* All this function will do is calculate:
   1. penalty_move
   2. penalty_stay
   3. penalty_displace
   For the set of sites that the hotset would move from the upper to the lower
   tier, and vice versa. Sites whose current tier agrees with the current hotset
   are ignored.
*/
void prepare_stats_ski(tree(site_info_ptr, int) sorted_sites) {
  tree_it(site_info_ptr, int) sit;
  int index;
  char dev, hot, prev_hot;
  
  /* We'll assume that the value at index 0 is the number of accesses to DRAM,
     and the value at index 1 is the number of accesses to AEP. Thus, we're
     assuming that:
     SH_PROFILE_PEBS_EVENTS=
       "MEM_LOAD_UOPS_LLC_MISS_RETIRED:LOCAL_DRAM,MEM_LOAD_UOPS_RETIRED:LOCAL_PMM"
  */

  get_online_data()->ski->penalty_move = 0;
  get_online_data()->ski->penalty_stay = 0;
  get_online_data()->ski->penalty_displace = 0;
  
  tree_traverse(sorted_sites, sit) {
    index = tree_it_key(sit)->index;
    dev = get_online_arena_prof(index)->dev;
    hot = get_online_arena_prof(index)->hot;
    
    /* Here, we'll only look at sites that the hotset would rebind */
    if(((dev == -1) && hot) ||
       ((dev == 0)  && hot) ||
       ((dev == 1)  && !hot)) {
         
      if(profopts.profile_online_debug_file) {
        fprintf(profopts.profile_online_debug_file,
                "Calculating penalties for site %d, dev: %d, hot: %d\n",
                tree_it_val(sit), dev, hot);
      }
         
      /* Calculate the penalties for this sites */
      get_online_data()->ski->penalty_move += penalty_move(index);
      if((((dev == -1) || (dev == 0)) && (hot))) {
        /* The site is due to be rebound up */
        get_online_data()->ski->penalty_stay += penalty_stay(index);
      } else if((dev == 1) && (!hot)) {
        /* The site is due to be rebound down */
        get_online_data()->ski->penalty_displace += penalty_displace(index);
      }
    }
  }
  
  if(profopts.profile_online_debug_file) {
    fprintf(profopts.profile_online_debug_file,
            "Total penalties: move: %zu, stay: %zu, displace: %zu\n",
            get_online_data()->ski->penalty_move,
            get_online_data()->ski->penalty_stay,
            get_online_data()->ski->penalty_displace);
    fflush(profopts.profile_online_debug_file);
  }
}

/* This implements the classic ski rental break-even algorithm. */
void profile_online_interval_ski(tree(site_info_ptr, int) sorted_sites) {
  size_t rent_cost, buy_cost, i,
         weight, prev_weight, weight_diff, bound_weight;
  char dev, hot, prev_hot;
  int index;
  tree_it(site_info_ptr, int) sit;
  site_info_ptr tmp;
  
  //suspend_all_threads();
  
  //print_smaps_info();
  
  /* The cost to "rent the skis" for another day is just penalty_stay;
     the amount of time that we, approximately, lose for keeping the "hot"
     sites in the lower tier. */
  rent_cost = get_online_data()->ski->penalty_stay;
     
  /* The cost to "buy the skis" is penalty_move plus penalty_displace; that is,
     the cost to actually do the `mbind` calls (which incurs significant overhead)
     plus the (presumably small) cost of keeping previously "hot" sites in the
     lower tier. */
  buy_cost = get_online_data()->ski->penalty_move +
             get_online_data()->ski->penalty_displace;
             
  /* Calculate weight_diff, which is the difference in weight between the currently-bound
     hotset and what those same sites weigh right now. */
  weight_diff = 0;
  bound_weight = 0;
  if(get_online_data()->cur_sorted_sites) {
    tree_traverse(get_online_data()->cur_sorted_sites, sit) {
      index = tree_it_key(sit)->index;
      prev_hot = tree_it_key(sit)->hot;
      prev_weight = tree_it_key(sit)->weight;
      hot = get_online_arena_prof(index)->hot;
      weight = get_online_arena_prof(index)->weight;
      
      bound_weight += weight;
      if((prev_hot != hot) && (weight > prev_weight)) {
        /* Only increment this if the site has changed in hotness, AND
          if the site has grown (shrinking sites won't push things out of the upper tier) */
        weight_diff += weight - prev_weight;
      }
    }
  }
     
  if(get_online_data()->first_online_interval == 1) {
    /* Here, we'll short-circuit the algorithm and rebind all sites no matter what, since
       the online approach has just now taken over. */
    if(profopts.profile_online_debug_file) {
      fprintf(profopts.profile_online_debug_file, "First online interval. Rebinding.\n");
    }
    full_rebind(sorted_sites);
    //print_smaps_info();
  } else if((get_online_data()->first_online_interval == 2) && (rent_cost > 0.0) && (rent_cost >= buy_cost)) {
    /* This is the case where it's not our first online interval, but the rent cost exceeds the cost to buy. */
    full_rebind(sorted_sites);
    //print_smaps_info();
  } else if((get_online_data()->first_online_interval == 2) && weight_diff && (bound_weight > get_online_data()->upper_max)) {
    /* In this case, we only want to rebind if:
       1. The online approach has taken over.
       2. The sites which are currently bound to the upper tier have GROWN in size.
       3. The sites which are currently bound to the upper tier are now overpacking the upper tier. */
    if(profopts.profile_online_debug_file) {
      fprintf(profopts.profile_online_debug_file, "Sites grew by %zu bytes out from under us, and now overpack (%zu / %zu). Rebinding.\n", weight_diff, bound_weight, get_online_data()->upper_max);
    }
    full_rebind(sorted_sites);
  } else {
    get_online_prof()->reconfigure = 0;
    
    /* Since we only update these values when there aren't any rebindings, prev_bw will be the bandwidth
       before the last rebinding here */
    update_bw();
    #if 0
    if(bw_tanked() && get_online_data()->prev_interval_reconfigure) {
      if(profopts.profile_online_debug_file) {
        fprintf(profopts.profile_online_debug_file, "%zu->%zu, so rebinding the old hotset.\n",
          get_online_data()->prev_bw, get_online_data()->cur_bw);
      }
      full_rebind(get_online_data()->prev_sorted_sites);
    } else if(get_online_data()->prev_interval_reconfigure) {
      if(profopts.profile_online_debug_file) {
        fprintf(profopts.profile_online_debug_file, "%zu->%zu, so the rebinding was good.\n",
          get_online_data()->prev_bw, get_online_data()->cur_bw);
      }
      get_online_data()->prev_interval_reconfigure = 0;
    }
    #endif
  }
  //check_rebind();
  //resume_all_threads();
}
