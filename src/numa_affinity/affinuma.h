#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <stddef.h>
#include <ctype.h>
#include <unistd.h>
#include <time.h>
#include <math.h>
#include <sched.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/sysinfo.h>
#include "numa.h"

#define BILLION  1000000000L;

struct numa_node_bw{
	int numa_id;
	char * mem_type;
	long double wr_only_avg;
        long double owtr_avg;
	struct numa_node_bw * next;
};

struct numa_node_bw * numa_node_list;
struct numa_node_bw * numa_list_head;
int mem_types;
int max_node;
int numt;
int total_numa_nodes;
int * numa_node_ids;
struct bitmask * numa_nodes;
char ** mem_tech;
long double * means;
int * cluster_sizes;

void label_mem();
void sort_list(struct numa_node_bw * new_node);
void write_config_file();
void numatest(int argc, char ** argv);
void gputest(int argc, char ** argv);
