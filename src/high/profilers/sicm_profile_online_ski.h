/*******************
  The `ski` online strategy.
*******************/

/* This is the amount of time, in milliseconds, that it will
    take to do the complete rebinding. */
static inline size_t penalty_move(size_t cap) {
  size_t retval;
  
  retval = ((double) cap)
           / 2097152; /* 2 GB/s, converted to bytes/ms */
  if(retval < 50) {
    retval = 50;
  }
  return retval;
}

/* This is the amount of time in milliseconds that we've wasted thus
   far by keeping `accesses` number of accesses in the AEP */
static inline size_t penalty_stay(size_t accesses) {
  size_t retval;
  
  retval =  (accesses)
            * 200 /* PEBS only gets 0.05% of accesses on average */
            * 0.0003; /* We lose 300 nanoseconds per access, to ms */
  return retval;
}

/* This is the amount of time in milliseconds that we *would* waste by
   displacing this site in the upper tier (that is, moving it from the upper
   tier to the lower tier) */
static inline size_t penalty_displace(size_t accesses) {
  /* This value is calculated the same way, just with different
     accesses */
  return penalty_stay(accesses);
}

void profile_online_init_ski() {
  prof.profile_online.ski = orig_malloc(sizeof(profile_online_data_ski));
  #if 0
  if(!(should_profile_bw())) {
    fprintf(stderr, "SH_PROFILE_ONLINE_STRAT_SKI requires SH_PROFILE_BW. Aborting.\n");
    exit(1);
  }
  #endif
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
  size_t num_hot_intervals;
  int index;
  char dev, hot, prev_hot;
  size_t pen_move, pen_stay, pen_dis;
  
  /* We'll assume that the value at index 0 is the number of accesses to DRAM,
     and the value at index 1 is the number of accesses to AEP. Thus, we're
     assuming that:
     SH_PROFILE_ALL_EVENTS=
       "MEM_LOAD_UOPS_LLC_MISS_RETIRED:LOCAL_DRAM,MEM_LOAD_UOPS_RETIRED:LOCAL_PMM"
  */

  prof.profile_online.ski->penalty_move = 0;
  prof.profile_online.ski->penalty_stay = 0;
  prof.profile_online.ski->penalty_displace = 0;
  
  tree_traverse(sorted_sites, sit) {
    index = tree_it_key(sit)->index;
    dev = get_arena_online_prof(index)->dev;
    hot = get_arena_online_prof(index)->hot;
    
    /* Here, we'll only look at sites that the hotset would rebind */
    if(((dev == -1) && hot) ||
       ((dev == 0)  && hot) ||
       ((dev == 1)  && !hot)) {
         
      /* Calculate the penalty to move this site */
      pen_move = penalty_move(tree_it_key(sit)->weight);
      prof.profile_online.ski->penalty_move += pen_move;
      
      if((((dev == -1) || (dev == 0)) && (hot))) {
        /* The site is due to be rebound up */
        pen_stay = penalty_stay(tree_it_key(sit)->value_arr[1]);
        prof.profile_online.ski->penalty_stay += pen_stay;
      } else if((dev == 1) && (!hot)) {
        /* The site is due to be rebound down */
        pen_dis = penalty_displace(tree_it_key(sit)->value_arr[0]);
        prof.profile_online.ski->penalty_displace += pen_dis;
      }
    }
  }
  
  if(profopts.profile_online_debug_file) {
    fprintf(profopts.profile_online_debug_file,
            "Total penalties: move: %zu, stay: %zu, displace: %zu\n",
            prof.profile_online.ski->penalty_move,
            prof.profile_online.ski->penalty_stay,
            prof.profile_online.ski->penalty_displace);
    fprintf(profopts.profile_online_debug_file,
            "Interval: %zu\n", prof.profile->num_intervals - 1);
  }
}

/* This implements the classic ski rental break-even algorithm. */
void profile_online_interval_ski(tree(site_info_ptr, int) sorted_sites) {
  size_t rent_cost, buy_cost;
  char dev;
  int index;
  tree_it(site_info_ptr, int) sit;
  
  /* The cost to "rent the skis" for another day is just penalty_stay;
     the amount of time that we, approximately, lose for keeping the "hot"
     sites in the lower tier. */
  rent_cost = prof.profile_online.ski->penalty_stay;
     
  /* The cost to "buy the skis" is penalty_move plus penalty_displace; that is,
     the cost to actually do the `mbind` calls (which incurs significant overhead)
     plus the (presumably small) cost of keeping previously "hot" sites in the
     lower tier. */
  buy_cost = prof.profile_online.ski->penalty_move +
             prof.profile_online.ski->penalty_displace;
     
  /* We rebind everything to match the current hotset if the cumulative
     cost of "renting" exceeds the cost to "buy." */
  if((rent_cost > 0.0) && (rent_cost >= buy_cost)) {
    get_profile_online_prof()->reconfigure = 1;
    full_rebind(sorted_sites);
  } else {
    get_profile_online_prof()->reconfigure = 0;
  }
}
