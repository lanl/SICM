#pragma once
#include "sicm_profile.h"

#if 0
void online_reconfigure() {
  size_t i, packed_size, total_value;
  double acc_per_byte;
  tree(double, size_t) sorted_arenas;
  tree(size_t, deviceptr) new_knapsack;
  tree_it(double, size_t) it;
  tree_it(size_t, deviceptr) kit;
  tree_it(int, deviceptr) sit;
  char break_next_site;
  int id;

  printf("===== STARTING RECONFIGURING =====\n");
  /* Sort all sites by accesses/byte */
  sorted_arenas = tree_make(double, size_t); /* acc_per_byte -> arena index */
  packed_size = 0;
  for(i = 0; i <= max_index; i++) {
    if(!arenas[i]) continue;
    if(arenas[i]->peak_rss == 0) continue;
    if(arenas[i]->accesses == 0) continue;
    acc_per_byte = ((double)arenas[i]->accesses) / ((double) arenas[i]->peak_rss);
    it = tree_lookup(sorted_arenas, acc_per_byte);
    while(tree_it_good(it)) {
      /* Inch this site a little higher to avoid collisions in the tree */
      acc_per_byte += 0.000000000000000001;
      it = tree_lookup(sorted_arenas, acc_per_byte);
    }
    tree_insert(sorted_arenas, acc_per_byte, i);
  }

  /* Use a greedy algorithm to pack sites into the knapsack */
  total_value = 0;
  break_next_site = 0;
  new_knapsack = tree_make(size_t, deviceptr); /* arena index -> online_device */
  it = tree_last(sorted_arenas);
  while(tree_it_good(it)) {
    packed_size += arenas[tree_it_val(it)]->peak_rss;
    total_value += arenas[tree_it_val(it)]->accesses;
    tree_insert(new_knapsack, tree_it_val(it), online_device);
    for(id = 0; id < arenas[tree_it_val(it)]->num_alloc_sites; id++) {
      printf("%d ", arenas[tree_it_val(it)]->alloc_sites[id]);
    }
    if(break_next_site) {
      break;
    }
    if(packed_size > online_device_cap) {
      break_next_site = 1;
    }
    tree_it_prev(it);
  }
  printf("\n");
  printf("Total value: %zu\n", total_value);
  printf("Packed size: %zu\n", packed_size);
  printf("Capacity:    %zd\n", online_device_cap);

  /* Get rid of sites that aren't in the new knapsack but are in the old */
  tree_traverse(site_nodes, sit) {
    i = get_arena_index(tree_it_key(sit));
    kit = tree_lookup(new_knapsack, i);
    if(!tree_it_good(kit)) {
      /* The site isn't in the new, so remove it from the upper tier */
      tree_delete(site_nodes, tree_it_key(sit));
      sicm_arena_set_device(arenas[i]->arena, default_device);
      printf("Moving %d out of the MCDRAM\n", tree_it_key(sit));
    }
  }

  /* Add sites that weren't in the old knapsack but are in the new */
  tree_traverse(new_knapsack, kit) {
    for(id = 0; id < arenas[tree_it_key(kit)]->num_alloc_sites; id++) {
      /* Lookup this site in the old knapsack */
      sit = tree_lookup(site_nodes, arenas[tree_it_key(kit)]->alloc_sites[id]);
      if(!tree_it_good(sit)) {
        /* This site is in the new but not the old */
        tree_insert(site_nodes, arenas[tree_it_key(kit)]->alloc_sites[id], online_device);
        sicm_arena_set_device(arenas[tree_it_key(kit)]->arena, online_device);
        printf("Moving %d into the MCDRAM\n", arenas[tree_it_key(kit)]->alloc_sites[id]);
      }
    }
  }

  tree_free(sorted_arenas);
}
#endif
