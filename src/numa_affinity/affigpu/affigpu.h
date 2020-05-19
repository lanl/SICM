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


void label_mem();
void sort_list(struct numa_node_bw * new_node);
void write_config_file();
void gputest(int argc, char ** argv);
