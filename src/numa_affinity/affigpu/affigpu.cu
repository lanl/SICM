#include "affigpu.h"

struct numa_node_bw{
	int numa_id;
	int device_id;
	char * mem_type;
	long double wr_only_avg;
        long double owtr_avg;
	struct numa_node_bw * next;
};

struct numa_node_bw * numa_node_list=NULL;
struct numa_node_bw * numa_list_head=NULL;
int mem_types;
int max_node;
int numt;
int total_numa_nodes=0;
int * numa_node_ids;
struct bitmask * numa_nodes;
char ** mem_tech;
long double * means;
int * cluster_sizes;
char classes[3][8] = {"fast", "slow", "slowest"};

__global__ void gpu_buffer(double *a, double *b, double *c, int n){
	int j;
	for(j =0; j < n; j++){
		a[j] = c[j] + b[j];
	}
}


void label_mem(){
	struct numa_node_bw * bw_it = numa_list_head;
	struct numa_node_bw * next_bw_it = bw_it->next;
	int i = 0;
	bw_it->mem_type = classes[i];
	
	while(next_bw_it != NULL){
		long double diff = bw_it->owtr_avg - next_bw_it->owtr_avg;
		long double perct = 0.2*bw_it->owtr_avg;
		if((diff > perct)&&((i+1)<3)){
			i++;
		}
		next_bw_it->mem_type = classes[i];
		bw_it = next_bw_it;
		next_bw_it= bw_it->next;
	}
}

void sort_list(struct numa_node_bw * new_node){
	struct numa_node_bw * bw_it = numa_list_head;
	struct numa_node_bw * prev_bw_it = NULL;
	while(bw_it != NULL){
		if((bw_it->owtr_avg < new_node->owtr_avg)){
			if(prev_bw_it == NULL){
				new_node->next = bw_it;
				numa_list_head = new_node;
			}else{
				prev_bw_it->next = new_node;
				new_node->next = bw_it;
			}
			return;
		}
		prev_bw_it = bw_it;
		bw_it = bw_it->next;
	}
	prev_bw_it->next = new_node;
	return;

}

void write_config_file(){
	FILE * conf;
	char fname[50];
	strcpy(fname, "numa_class");
	conf = fopen(fname, "w");
	struct numa_node_bw * bw_it = numa_list_head;
	printf("GPU ID\tNUMA ID\tType\tInit(Mb/s)\tTriad(Mb/s)\n");
	while(bw_it != NULL){	
		fprintf(conf, "%d %d %s %Lf %Lf\n", bw_it->device_id, bw_it->numa_id, bw_it->mem_type, bw_it->wr_only_avg, bw_it->owtr_avg);
		printf("%d\t%d\t%s\t%Lf\t%Lf\n",bw_it->device_id, bw_it->numa_id, bw_it->mem_type, bw_it->wr_only_avg, bw_it->owtr_avg);
		bw_it = bw_it->next;
	}
	fclose(conf);
}

void gputest(int argc, char ** argv){
	max_node = numa_max_node() + 1;
	int cpu_count = numa_num_possible_cpus();
	numa_node_ids = (int*)malloc(sizeof(int)*max_node);
	struct bitmask * numa_nodes = numa_get_membind();
	int i = 0;
	while(i < numa_nodes->size){
		if(numa_bitmask_isbitset(numa_nodes, i)){
			numa_node_ids[total_numa_nodes] = i;
			total_numa_nodes++;
		}
		i++;
	}

	int ndevs;
	cudaGetDeviceCount(&ndevs);
	int mbs = 64;
	unsigned long size = mbs*1024*1024;
	double *a, *b, *c, *ag, *bg, *cg;
	struct timespec begin, stop;
	srand(clock());
	//sleep(10);
 
 	i = 0;
	while(i < ndevs){
		cudaSetDevice(i);
		int iters = 0;
		int k=0;
		long double wr_only_avg=0.0;
		long double owtr_avg=0.0;
		long double accum;
	while(k < total_numa_nodes){
		for( iters = 0; iters < 10; iters++){
			int j = 0;
			a = (double*)numa_alloc_onnode(size, numa_node_ids[k]);
			b = (double*)numa_alloc_onnode(size, numa_node_ids[k]);
			c = (double*)numa_alloc_onnode(size, numa_node_ids[k]);
			cudaMalloc(&ag, size);
			cudaMalloc(&bg, size);
			cudaMalloc(&cg, size);
			long double empty=0.0;
			clock_gettime( CLOCK_MONOTONIC, &begin);
			for(j = 0;j < (size/sizeof(double));j++){
				a[j] = 1.0;
				b[j] = 2.0;
				c[j] = 3.0;
			}
			cudaMemcpy(ag, a, size, cudaMemcpyHostToDevice);
			cudaMemcpy(bg, b, size, cudaMemcpyHostToDevice);
			cudaMemcpy(cg, c, size, cudaMemcpyHostToDevice);
			clock_gettime( CLOCK_MONOTONIC, &stop);
			accum = ( stop.tv_sec - begin.tv_sec ) + (long double)( stop.tv_nsec - begin.tv_nsec ) / (long double)BILLION;
			wr_only_avg += ((3*size*1.0E-06)/(long double)(accum - empty));
			clock_gettime( CLOCK_MONOTONIC, &begin);
			gpu_buffer<<<((size/sizeof(double))+255)/256, 256>>>(ag,bg,cg,(size/sizeof(double)));
			clock_gettime( CLOCK_MONOTONIC, &stop);
			accum = ( stop.tv_sec - begin.tv_sec ) + (long double)( stop.tv_nsec - begin.tv_nsec ) / (long double)BILLION;
			owtr_avg += ((3*size*1.0E-06)/(long double)(accum - empty));
			cudaFree(ag);
			cudaFree(bg);
			cudaFree(cg);
			numa_free(a, size);
			numa_free(b, size);
			numa_free(c, size);
		}
		struct numa_node_bw * node_bw = (struct numa_node_bw *)malloc(sizeof(struct numa_node_bw));
		node_bw->device_id = i;
		node_bw->numa_id = numa_node_ids[k];
		node_bw->wr_only_avg = wr_only_avg/10;
		node_bw->owtr_avg = owtr_avg/10;
		node_bw->next = NULL;
		if(numa_node_list == NULL){
			numa_node_list = node_bw;
			numa_list_head = numa_node_list;
		}
		else{
			sort_list(node_bw);
		}
		k++;
	}
		i++;
	}
	label_mem();
	write_config_file();
}
