/*******************
  The `ski` online strategy.
*******************/

/* This is the amount of time, in microseconds, that it will
    take to do the complete rebinding. */
static inline double penalty_move(size_t cap) {
  double retval;
  
  retval = ((double) cap)
           / 2097152; /* 2GB/s, converted to bytes/ms */
  return retval;
}

/* This is the amount of time in milliseconds that we've wasted by keeping
   `accesses` number of accesses in the AEP */
static inline double penalty_stay(size_t accesses) {
  double retval;
  
  retval =  ((double) accesses)
            * 200 /* PEBS only gets 0.05% of accesses on average */
            * 0.0003; /* We lose 300 nanoseconds per access, to ms */
  return retval;
}

static inline size_t penalty_displace(size_t accesses) {
  /* This value is calculated the same way, just with different
     accesses */
  return penalty_stay(accesses);
}

void profile_online_init_ski() {
  prof.profile_online.ski = malloc(sizeof(profile_online_data_ski));
}

void prepare_stats_ski(tree(site_info_ptr, int) sorted_sites) {
  tree_it(site_info_ptr, int) sit;
  size_t num_hot_intervals;
  int index;
  char dev, hot, prev_hot;
  double pen_move, pen_stay, pen_dis;
  
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
    
    /* Calculate what would have to be rebound if the current hotset
       were to trigger a full rebind */
    if(((dev == -1) && hot) ||
       ((dev == 0)  && hot) ||
       ((dev == 1)  && !hot)) {
        /* If the site is in the hotset, but not in the upper tier, OR
           if the site is not in the hotset, but in the upper tier */
        pen_move = penalty_move(tree_it_key(sit)->weight);
        prof.profile_online.ski->penalty_move += pen_move;
        if(((dev == -1) || (dev == 0)) && (hot)) {
          /* The site is due to be rebound up */
          pen_stay = penalty_stay(tree_it_key(sit)->value_arr[1]);
          prof.profile_online.ski->penalty_stay += pen_stay;
        } else if((dev == 1) && (!hot)) {
          /* The site is due to be rebound down */
          pen_dis = penalty_displace(tree_it_key(sit)->value_arr[0]);
          prof.profile_online.ski->penalty_displace += pen_dis;
        }
        if(profopts.profile_online_debug_file) {
          fprintf(profopts.profile_online_debug_file,
                  "%d: %lf %lf %lf\n", index, pen_move, pen_stay, pen_dis);
        }
    }
  }
  fprintf(profopts.profile_online_debug_file,
          "Total penalties: %lf %lf %lf\n",
          prof.profile_online.ski->penalty_move,
          prof.profile_online.ski->penalty_stay,
          prof.profile_online.ski->penalty_displace);
}

void profile_online_interval_ski(tree(site_info_ptr, int) sorted_sites) {
  arena_info *arena;
  arena_profile *aprof;
  sicm_dev_ptr dl;
  int index;
  char full_rebind, dev, hot, prev_hot;
  size_t num_hot_intervals;

  tree_it(site_info_ptr, int) sit;

  full_rebind = 0;
  if(!profopts.profile_online_nobind &&
     prof.profile_online.upper_contention &&
     (prof.profile_online.ski->total_site_value > profopts.profile_online_grace_accesses) &&
     ((((float) prof.profile_online.ski->site_weight_to_rebind) / ((float) prof.profile_online.ski->total_site_weight)) >= profopts.profile_online_reconf_weight_ratio)) {
    /* Do a full rebind. Take the difference between what's currently on the devices (site_tiers),
       and what the hotset says should be on there. */
    tree_traverse(sorted_sites, sit) {
      index = tree_it_key(sit)->index;
      dev = get_arena_online_prof(index)->dev;
      hot = get_arena_online_prof(index)->hot;
      if(prof.profile->num_intervals) {
        prev_hot = get_prev_arena_online_prof(index)->hot;
      } else {
        prev_hot = 0;
      }

      dl = NULL;
      if(((dev == -1) && hot) ||
         ((dev == 0) && hot)) {
        /* The site is in AEP, and is in the hotset. */
        dl = prof.profile_online.upper_dl;
        get_arena_online_prof(index)->dev = 1;
      } else if((dev == 1) && (hot == 0)) {
        /* The site is in DRAM and isn't in the hotset */
        dl = prof.profile_online.lower_dl;
        get_arena_online_prof(index)->dev = 0;
      }

      if(dl) {
        full_rebind = 1;

        rebind_arena(index, dl, sit);
      }
    }
  } else {
    /* No full rebind, but we can bind specific sites if the conditions are right */
    if(profopts.profile_online_hot_intervals) {
      /* If the user specified a number of intervals, rebind the sites that
         have been hot for that amount of intervals */
      tree_traverse(sorted_sites, sit) {
        index = tree_it_key(sit)->index;
        num_hot_intervals = get_arena_online_prof(index)->num_hot_intervals;
        if(num_hot_intervals == profopts.profile_online_hot_intervals) {
          get_arena_online_prof(index)->dev = 1;
          rebind_arena(index, prof.profile_online.upper_dl, sit);
        }
      }
    }
  }
}
