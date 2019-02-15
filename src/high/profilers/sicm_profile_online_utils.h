/* Rebinds an arena to the device list.
   Times it and prints debugging information if necessary. */
void rebind_arena(int index, sicm_dev_ptr dl, tree_it(site_info_ptr, int) sit) {
  int retval;
  struct timespec start, end, diff;

  if(profopts.profile_online_debug_file) {
    clock_gettime(CLOCK_MONOTONIC, &start);
  }

  retval = sicm_arena_set_devices(tracker.arenas[index]->arena, dl);

  if(profopts.profile_online_debug_file) {
    clock_gettime(CLOCK_MONOTONIC, &end);
    timespec_diff(&start, &end, &actual);
    fprintf(profopts.profile_online_debug_file,
      "Rebinding %zu bytes ", tree_it_key(sit)->weight);
    if(dl == prof.profile_online.upper_dl) {
      fprintf(profopts.profile_online_debug_file,
        "AEP->DRAM ");
    } else {
      fprintf(profopts.profile_online_debug_file,
        "DRAM->AEP ");
    }
    fprintf(profopts.profile_online_debug_file,
      "took %ld.%09ld seconds.\n", actual.tv_sec, actual.tv_nsec);

    if(retval == -EINVAL) {
      fprintf(profopts.profile_online_debug_file,
        "Rebinding arena %d failed in SICM.\n", index);
    } else if(retval != 0) {
      fprintf(profopts.profile_online_debug_file,
        "Rebinding arena %d failed internally.\n", index);
    }
  }
}
