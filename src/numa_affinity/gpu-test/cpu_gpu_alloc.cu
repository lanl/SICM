#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <stdint.h>
#include <ctype.h>
#include <unistd.h>
#include <time.h>
#include <math.h>
#include <sched.h>
#include <sys/time.h>
#include "numa.h"

__global__
void saxpy(volatile int *arr, int n)
{
  int i = blockIdx.x*blockDim.x + threadIdx.x;
  if (i < n) arr[i] = arr[i] + 70*i;
}

void nmc(volatile int *arr, long n)
{
	int i = 0;
	srand(time(NULL));
   for(i = 0; i <n; i+=64)
   	arr[i] = arr[i] + (int)(70*rand()/i)%100;
}

int main(){
	int max_node;
	int total_numa_nodes = 0;
	int * numa_node_ids;
	max_node = numa_max_node() + 1;
	int cpu_count = numa_num_possible_cpus();
	numa_node_ids = (int*)malloc(sizeof(int)*max_node);
	struct bitmask * numa_nodes = numa_get_membind();
	size_t * fr;
	size_t * tot;
	int n = 0;
	while(n < numa_nodes->size){
		if(numa_bitmask_isbitset(numa_nodes, n)){
			numa_node_ids[total_numa_nodes] = n;
			total_numa_nodes++;
		}
		n++;
	}

	cudaDeviceProp cdp;
	int mt;

        volatile int * arr1;
        volatile int * arr2;
	int j=-1;
	int k= 0;
	int err=-1;
	long numa_avail;
	size_t total = 0;
	n = 0;
	cudaGetDeviceCount(&j);
	fr = (size_t *)malloc(j*sizeof(size_t));
	tot = (size_t *)malloc(j*sizeof(size_t));
	for(k= 0; k < j; k++){
		cudaSetDevice(k);
		cudaMemGetInfo(&fr[k], &tot[k]);
		if(tot[k] > total)
			total = tot[k];
	}
	while(n < total_numa_nodes){
		numa_avail = numa_node_size(numa_node_ids[n], NULL);
		if(numa_avail < total){
			arr1 = (volatile int *)numa_alloc_onnode(numa_avail/2, numa_node_ids[n]);
			nmc(arr1, numa_avail/(2*sizeof(int)));
		}else{
			arr1 = (volatile int *)numa_alloc_onnode(total/2, numa_node_ids[n]);
			nmc(arr1, total/(2*sizeof(int)));
		}
		for(k=0;k<j;k++){
			cudaSetDevice(k);
			cudaGetDeviceProperties(&cdp, k);
			mt = cdp.maxThreadsPerBlock;
			err = cudaMalloc(&arr2, (tot[k]/2));
			if((err == 2)){
				printf("GPGPU: %d, Numa id: %d\n",k, numa_node_ids[n] );
			}
			saxpy<<<mt/32, mt>>>(arr2, (tot[k]/2)/sizeof(int));
       			cudaFree((void *)arr2);
		}

		if(numa_avail < total)
			numa_free((void *)arr1, numa_avail/2);
		else
			numa_free((void *)arr1, total/2);
		n++;
	}
        return 1;
}
