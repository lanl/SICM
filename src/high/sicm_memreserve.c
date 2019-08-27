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
	char *data, *ptr, ***ranges, hold;
	unsigned long long amount;
	long long freemem;
	int strict, node;
	
	if(argc != 6) {
		fprintf(stderr, "USAGE: ./memreserve [node] [num_threads] [amount] ['hold','release'] ['prefer','bind']\n");
		fprintf(stderr, "node: the node to reserve memory on\n");
		fprintf(stderr, "num_threads: the number of threads to use to fill in the memory with garbage data\n");
		fprintf(stderr, "amount: the amount of pages to reserve.\n");
		fprintf(stderr, "hold/release: 'hold' to pause after allocating, 'release' to let go after fulfilling the allocation.\n");
		fprintf(stderr, "prefer,bind: choose between 'prefer'ing the numa node or 'bind'ing to it strictly.\n");
		exit(1);
	}

	/* Read in arguments */
	node = (int) strtol(argv[1], NULL, 0);
	num_threads = strtoumax(argv[2], NULL, 0);
  amount = strtoumax(argv[3], NULL, 0);
	if(strcmp(argv[4], "hold") == 0) {
		hold = 1;
	} else if(strcmp(argv[4], "release") == 0) {
		hold = 0;
	} else {
		fprintf(stderr, "Fourth argument is not 'hold' or 'release'. Aborting.\n");
		exit(1);
	}
	if(strcmp(argv[5], "prefer") == 0) {
		strict = 0;
	} else if(strcmp(argv[5], "bind") == 0) {
		strict = 1;
	} else {
		fprintf(stderr, "Fifth argument is not 'prefer' or 'bind'. Aborting.\n");
		exit(1);
	}

	/* Now get the number of pages that we need to reserve */
	pagesize = (uintmax_t) getpagesize();
  num_pages = amount;
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
	numa_free(data, size);
	return 0;
}
