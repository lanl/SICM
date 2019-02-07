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
#include "sicm_parsing.h"

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
	size_t i, num_pages, size, pagesize,
				 pages_per_thread, runoff, num_threads;
	pthread_t *threads;
	char *data, *ptr, ***ranges, captype, hold;
	float ratio;
	unsigned long long constant;
	long long freemem;
	app_info *info;
	int strict, node;
	
	if(argc != 7) {
		fprintf(stderr, "USAGE: ./memreserve [node] [num_threads] ['ratio','constant'] [value] ['hold','release'] ['prefer','bind']\n");
		fprintf(stderr, "node: the node to reserve memory on\n");
		fprintf(stderr, "num_threads: the number of threads to use to fill in the memory with garbage data\n");
		fprintf(stderr, "ratio,constant: choose between reserving *all but* a 'ratio' of the application's peak RSS or a constant number of pages\n");
		fprintf(stderr, "value: either the float (if 'ratio') or the constant number of pages (if 'constant').\n");
		fprintf(stderr, "hold/release: 'hold' to pause after allocating, 'release' to let go after fulfilling the allocation.\n");
		fprintf(stderr, "prefer,bind: choose between 'prefer'ing the numa node or 'bind'ing to it strictly.\n");
		fprintf(stderr, "If 'ratio', provide profiling information about the application on 'stdin'.\n");
		exit(1);
	}

	/* Read in arguments */
	node = (int) strtol(argv[1], NULL, 0);
	num_threads = strtoumax(argv[2], NULL, 0);
	if(strcmp(argv[3], "ratio") == 0) {
		captype = 0;
		/* The next argument is a float */
		ratio = strtof(argv[4], NULL);
		if(ratio == 0.0) {
			return 0;
		}
	} else if(strcmp(argv[3], "constant") == 0) {
		captype = 1;
		constant = strtoumax(argv[4], NULL, 0);
		if(constant == 0) {
			return 0;
		}
	} else {
		fprintf(stderr, "Third argument not 'ratio' or 'constant'. Aborting.\n");
		exit(1);
	}
	if(strcmp(argv[5], "hold") == 0) {
		hold = 1;
	} else if(strcmp(argv[5], "release") == 0) {
		hold = 0;
	} else {
		fprintf(stderr, "Fifth argument is not 'hold' or 'release'. Aborting.\n");
		exit(1);
	}
	if(strcmp(argv[6], "prefer") == 0) {
		strict = 0;
	} else if(strcmp(argv[6], "bind") == 0) {
		strict = 1;
	} else {
		fprintf(stderr, "Sixth argument is not 'prefer' or 'bind'. Aborting.\n");
		exit(1);
	}

	/* Now get the number of pages that we need to reserve */
	pagesize = (uintmax_t) getpagesize();
	if(captype == 1) {
		num_pages = constant;
	} else {
		/* If it's a ratio, we need to parse the profiling information on stdin
		 * to get the total peak RSS of the application.
		 */
		info = sh_parse_site_info(stdin);
		printf("The peak RSS of the application is %zu.\n", info->peak_rss);
		numa_node_size64(node, &freemem);
		/* We want to allocate all pages *except* the ones that the application requires */
		if(freemem > (info->peak_rss * ratio)) {
			num_pages = (freemem - (info->peak_rss * ratio)) / pagesize;
		} else {
			num_pages = 0;//freemem / pagesize;
		}
	}

	size = num_pages * pagesize;
	
	/* Allocate the data and fill it with garbage */
	/* Uses numa to bind all of this process' memory to the node.
	 * Also uses bind, not preferred.
	 */
	printf("Reserving %zu bytes. ", size);
	if(strict) {
		printf("Binding strictly.\n");
	} else {
		printf("Preferring.\n");
	}
	numa_set_bind_policy(strict);
	data = numa_alloc_onnode(size, node);
	/* Old code used to use mbind, numa library calls are simpler
	syscall(237, data, size, node_info->mode, node_info->nodemask, node_info->maxnode, MPOL_MF_MOVE | MPOL_MF_STRICT);
	*/

	/* Allocate the array of threads and arguments */
	threads = malloc(sizeof(pthread_t) * num_threads);
	ranges = malloc(sizeof(char **) * num_threads);

	/* Set up the arguments */
	if(num_threads < num_pages) {
		pages_per_thread = num_pages / num_threads;
	} else {
		pages_per_thread = 1;
		num_threads = num_pages;
	}
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

	numa_node_size64(node, &freemem);
	printf("There is now %lld free memory on the MCDRAM.\n", freemem);
	
	if(hold) {
		printf("Holding memory until killed.\n");
		fflush(stdout);
		pause();
	}
	printf("Freeing up.\n");
	numa_free(data, size);
	return 0;
}
