/* Memreserve simply uses a number of threads to allocate
 * and reserve memory to a NUMA node for all eternity.
 * Intended to reserve memory on a NUMA node so that it can't
 * be used by another application.
 */

#include <stdio.h>
#include <inttypes.h>
#include <unistd.h>
#include <numa.h>
#include <numaif.h>
#include <pthread.h>
#include "sicm_high.h"

void *fill_pages(void *arg)
{
  char *i, **range;
  range = arg;

  i = (char *) range[0];
  while(i != range[1]) {
    *i = 'a';
    i++;
  }
}

int main(int argc, char **argv)
{
  uintmax_t i, num_pages, size, pagesize,
            pages_per_thread, runoff, num_threads;
  pthread_t *threads;
  char *data, *ptr, ***ranges, captype;
  float ratio;
  unsigned long long constant;
  size_t peak_rss;
  struct bitmask *nodemask;
  long long freemem;
  app_info *info;
  
  if(argc != 5) {
    fprintf(stderr, "USAGE: ./memreserve [node] [num_threads] ['ratio','constant'] [value]\n");
    fprintf(stderr, "node: the node to reserve memory on\n");
    fprintf(stderr, "num_threads: the number of threads to use to fill in the memory with garbage data\n");
    fprintf(stderr, "ratio,constant: choose between reserving *all but* a 'ratio' of the application's peak RSS or a constant number of bytes (rounded to pages)\n");
    fprintf(stderr, "value: either the float (if 'ratio') or the constant number of bytes (if 'constant').\n");
    fprintf(stderr, "if 'ratio', provide profiling information about the application on 'stdin'.\n");
    exit(1);
  }

  /* Read in arguments */
  num_threads = strtoumax(argv[2], NULL, 0);
  if(strcmp(argv[3], "ratio") == 0) {
    captype = 0;
    /* The next argument is a float */
    ratio = strtof(argv[4], NULL);
  } else if(strcmp(argv[3], "constant") == 0) {
    captype = 1;
    constant = strtoumax(argv[4], NULL, 0);
  } else {
    fprintf(stderr, "Third argument not 'ratio' or 'constant'. Aborting.\n");
    exit(1);
  }

  nodemask = numa_parse_nodestring(argv[1]);
  /* Now get the number of pages that we need to reserve */
  pagesize = (uintmax_t) getpagesize();
  if(captype == 1) {
    num_pages = constant;
  } else {
    /* If it's a ratio we need to parse the profiling information on stdin
     * to get the total peak RSS of the application.
     */
    printf("Reading from 'stdin', which should contain the profiling information.\n");
    info = sh_parse_site_info(stdin);
    peak_rss = sh_get_peak_rss(info);
    for(i = 0; i <= numa_max_node(); i++) {
       if(numa_bitmask_isbitset(nodemask, i)) {
         break;
       }
    }
    printf("Peak RSS: %zu\n", peak_rss);
    numa_node_size64(i, &freemem);
    /* We want to allocate all pages *except* the ones that the application requires */
    num_pages = (freemem - (peak_rss * ratio)) / pagesize;
  }

  printf("Allocating %llu pages.\n", num_pages);
  size = num_pages * pagesize;
  
  /* Allocate the data and fill it with garbage */
  /* Uses numa to bind all of this process' memory to the node.
   * Also uses bind, not preferred.
   */
  numa_set_bind_policy(1);
  numa_bind(nodemask);
  data = valloc(size);
  /* Old code used to use mbind, numa library calls are simpler
  syscall(237, data, size, node_info->mode, node_info->nodemask, node_info->maxnode, MPOL_MF_MOVE | MPOL_MF_STRICT);
  */

  /* Allocate the array of threads and arguments */
  threads = malloc(sizeof(pthread_t) * num_threads);
  ranges = malloc(sizeof(char **) * num_threads);

  /* Set up the arguments */
  pages_per_thread = num_pages / num_threads;
  runoff = num_pages % num_threads; /* Number of threads that get one more */
  ptr = data;
  for(i = 0; i < num_threads; i++) {
    ranges[i] = malloc(sizeof(char *) * 2);
    ranges[i][0] = ptr;

    if(i >= num_threads - runoff) { /* It's a runoff thread */
      ranges[i][1] = ranges[i][0] + (pages_per_thread * pagesize);
    } else {
      ranges[i][1] = ranges[i][0] + ((pages_per_thread - 1) * pagesize);
    }
    ptr = ranges[i][1] + 1;

    pthread_create(&(threads[i]), NULL, fill_pages, (void *) ranges[i]);
  }

  /* Wait for the threads to finish up, clean up */
  for(i = 0; i < num_threads; i++) {
    pthread_join(threads[i], NULL);
    free(ranges[i]);
  }
  free(ranges);
  free(threads);

  printf("Finished reserving.\n"); fflush(stdout);
  for(i = 0; i <= numa_max_node(); i++) {
     if(numa_bitmask_isbitset(nodemask, i)) {
       break;
     }
  }
  numa_node_size64(i, &freemem);
  printf("There is now %lld free memory on the MCDRAM.\n", freemem);
  
  pause();
}
