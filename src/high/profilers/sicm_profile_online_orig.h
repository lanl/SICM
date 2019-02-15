/*******************
  The `orig` online strategy.
*******************/

void profile_online_init_orig() {
  prof.profile_online.orig = malloc(sizeof(profile_online_data_orig));
}

void prepare_stats_orig(tree(site_info_ptr, int) sorted_sites) {
  tree_it(site_info_ptr, int) sit;
  size_t num_hot_intervals;
  int index;
  char dev, hot, prev_hot;

  prof.profile_online.orig->total_site_weight     = 0;
  prof.profile_online.orig->total_site_value      = 0;
  prof.profile_online.orig->total_sites           = 0;
  prof.profile_online.orig->site_weight_diff      = 0;
  prof.profile_online.orig->site_value_diff       = 0;
  prof.profile_online.orig->num_sites_diff        = 0;
  prof.profile_online.orig->site_weight_to_rebind = 0;
  prof.profile_online.orig->site_value_to_rebind  = 0;
  prof.profile_online.orig->num_sites_to_rebind   = 0;

  tree_traverse(sorted_sites, sit) {
    index = tree_it_key(sit)->index;
    dev = get_arena_online_prof(index)->dev;
    hot = get_arena_online_prof(index)->hot;
    if(prof.profile->num_intervals) {
      prev_hot = get_prev_arena_online_prof(index)->hot;
    } else {
      prev_hot = 0;
    }

    prof.profile_online.orig->total_site_weight += tree_it_key(sit)->weight;
    prof.profile_online.orig->total_site_value += tree_it_key(sit)->value;
    prof.profile_online.orig->total_sites++;

    if(hot) {
      get_arena_online_prof(index)->num_hot_intervals++;
    } else {
      get_arena_online_prof(index)->num_hot_intervals = 0;
    }

    /* Differences between hotsets */
    if((hot && !prev_hot) ||
       (!hot && prev_hot)) {
      /* The site will be rebound if a full rebind happens */
      prof.profile_online.orig->site_weight_diff += tree_it_key(sit)->weight;
      prof.profile_online.orig->site_value_diff += tree_it_key(sit)->value;
      prof.profile_online.orig->num_sites_diff++;
    }

    /* Calculate what would have to be rebound if the current hotset
       were to trigger a full rebind */
    if(((dev == -1) && hot) ||
       ((dev == 0)  && hot) ||
       ((dev == 1)  && !hot)) {
        /* If the site is in the hotset, but not in the upper tier, OR
           if the site is not in the hotset, but in the upper tier */
        prof.profile_online.orig->site_weight_to_rebind += tree_it_key(sit)->weight;
        prof.profile_online.orig->site_value_to_rebind += tree_it_key(sit)->value;
        prof.profile_online.orig->num_sites_to_rebind++;
    }
  }
}

void profile_online_interval_orig(tree(site_info_ptr, int) sorted_sites) {
  arena_info *arena;
  arena_profile *aprof;
  sicm_dev_ptr dl;
  int index;
  char full_rebind, dev, hot, prev_hot;
  size_t num_hot_intervals;
  struct timespec start, end, diff;

  tree_it(site_info_ptr, int) sit;

  full_rebind = 0;
  if(!profopts.profile_online_nobind &&
     prof.profile_online.upper_contention &&
     (prof.profile_online.orig->total_site_value > profopts.profile_online_grace_accesses) &&
     ((((float) prof.profile_online.orig->site_weight_to_rebind) / ((float) prof.profile_online.orig->total_site_weight)) >= profopts.profile_online_reconf_weight_ratio)) {
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

        rebind_arena(index, dl);
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
          rebind_arena(index, prof.profile_online.upper_dl);
        }
      }
    }
  }
}
