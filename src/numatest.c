#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <ctype.h>
#include <unistd.h>
#include <time.h>
#include <math.h>
#include <sched.h>
#include <sys/time.h>
#include "numa.h"

struct numa_node_bw{
	int numa_id;
	char * mem_type;
	double read_bw;
	double write_bw;
	double read_lat;
	double write_lat;
	double avg_lat;
	double avg_bw;
	struct numa_node_bw * next;
};

struct numa_node_bw * numa_node_list = NULL;
struct numa_node_bw * numa_list_head = NULL;
int mem_types;
int max_node;
int total_numa_nodes = 0;
int * numa_node_ids;
struct bitmask * numa_nodes;
char ** mem_tech;
double * means;
int * cluster_sizes;

void calculate_distances(){
//printf("calc_dist\n");
//fflush(NULL);
	int i;
        struct numa_node_bw * bw_it = numa_list_head;
	while(bw_it != NULL){
		i = 0;
		double delta = 999999999.9999999;
		while(i < mem_types){
			double dist = abs(sqrt(abs((means[i] - bw_it->avg_bw))*abs((means[i] - bw_it->avg_bw))));
//printf("Delta: %lf Dist: %lf\n", delta, dist);
			if(dist < delta){
				delta = dist;
				if(strcmp(bw_it->mem_type, mem_tech[i])!=0){
					if(((i-1)>=0)&&(strcmp(bw_it->mem_type, mem_tech[i-1])==0)){
						cluster_sizes[i-1]--;
						bw_it->mem_type = mem_tech[i];
						cluster_sizes[i]++;
					}
					else if(((i+1)<mem_types)&&(strcmp(bw_it->mem_type, mem_tech[i+1])==0)){
                                                cluster_sizes[i+1]--;
						bw_it->mem_type = mem_tech[i];
						cluster_sizes[i]++;
                                        }
					//bw_it->mem_type = mem_tech[i];
				}
			}
			i++;
		}
//		printf("Numa id:%d Type:%s BW:%lf\n",bw_it->numa_id, bw_it->mem_type, bw_it->avg_bw);
		bw_it = bw_it->next;
	}
}

void calculate_mean(){
//printf("calculate_mean\n");
//fflush(NULL);
	int i = 0;
	struct numa_node_bw * bw_it = numa_list_head;
	while(i < mem_types){
		int j = 0;
		means[i] = 0.0;
		while(j < cluster_sizes[i]){
			means[i] += bw_it->avg_bw;
			j++;
			bw_it = bw_it->next;
		}
		means[i] /= cluster_sizes[i];
//printf("Mean: %lf\n", means[i]);
		i++;
	}
	calculate_distances();
}

void classify(){
//printf("classify\n");
//fflush(NULL);
	int cluster_size;
	int last_cluster_size;
	/*if((total_numa_nodes%mem_types) == 0){
		cluster_size = (total_numa_nodes/mem_types);
		last_cluster_size = cluster_size;
	}
	else if((total_numa_nodes%mem_types) > (mem_types/2)){
		cluster_size = (total_numa_nodes/mem_types) + 1;
		last_cluster_size = total_numa_nodes%cluster_size;
	}
	else if((total_numa_nodes%mem_types) < (mem_types/2)){
		cluster_size = (total_numa_nodes/mem_types) - 1;
		last_cluster_size = total_numa_nodes%cluster_size;
	}
	else if((total_numa_nodes%mem_types) == (mem_types/2)){
                cluster_size = (total_numa_nodes/mem_types) + 1;
                last_cluster_size = total_numa_nodes%cluster_size;
        }
	else{
		cluster_size = (total_numa_nodes/mem_types);
		last_cluster_size = last_cluster_size - cluster_size;
	}*/
	cluster_size = total_numa_nodes/mem_types;
	last_cluster_size = cluster_size + (total_numa_nodes%mem_types);
	cluster_sizes = (int *)malloc(sizeof(int)*mem_types);
	means = (double *)(malloc(mem_types*sizeof(double)));
	struct numa_node_bw * bw_it = numa_list_head;
	int i = 0;
	while(i < mem_types){
		if(i == (mem_types - 1)){
			cluster_sizes[i] = last_cluster_size;
		}
		else{
			cluster_sizes[i] = cluster_size;
		}
		i++;
	}
//printf("be %d\n", cluster_size);
//fflush(NULL);
	i = 0;
	int j = 1;
	while(bw_it != NULL){
		bw_it->mem_type = mem_tech[i];
		if(j < cluster_sizes[i]){
			j++;
		}
		else{
			j = 1;
			i++;
		}
//		printf("%s\n",bw_it->mem_type);
		bw_it = bw_it->next;
	}
	bw_it = numa_list_head;
	i = 0;
	while(i < 10){
		calculate_mean();
		i++;
	}

}

void sort_list(struct numa_node_bw * new_node){
	struct numa_node_bw * bw_it = numa_list_head;
	struct numa_node_bw * prev_bw_it = NULL;
	while(bw_it != NULL){
//printf("insert %lf %lf\n", new_node->avg_bw, bw_it->avg_bw);
//fflush(NULL);
		//if((bw_it->write_bw > new_node->write_bw)&&(bw_it->read_bw > new_node->read_bw)){
		if((bw_it->avg_bw > new_node->avg_bw)){
//printf("insert1 %d\n", new_node->numa_id);
//fflush(NULL);
			if(prev_bw_it == NULL){
//printf("insert2 %d\n", new_node->numa_id);
//fflush(NULL);
				new_node->next = bw_it;
				numa_list_head = new_node;
			}else{
//printf("insert3 %d\n", new_node->numa_id);
//fflush(NULL);
				prev_bw_it->next = new_node;
				new_node->next = bw_it;
			}
			return;
		}
//printf("insert4 %d\n", new_node->numa_id);
//fflush(NULL);
		prev_bw_it = bw_it;
		bw_it = bw_it->next;
	}
	prev_bw_it->next = new_node;
//printf("insert5 %d\n", new_node->numa_id);
//fflush(NULL);
	return;

}

void write_config_file(){
	FILE * conf;
	conf = fopen("sicm_numa_config", "w");
	struct numa_node_bw * bw_it = numa_list_head;
	while(bw_it != NULL){	
		fprintf(conf, "%d %s %lf %lf %lf %.10lf %.10lf %.10lf\n",bw_it->numa_id, bw_it->mem_type, bw_it->avg_bw, bw_it->read_bw, bw_it->write_bw, bw_it->avg_lat, bw_it->read_lat, bw_it->write_lat);
		printf("Node id: %d Mem Type: %s Avg BW: %lf MB/s Read BW: %lf MB/s Write BW: %lf MB/s Avg Lat: %.10lf s Read Lat: %.10lf s Write Lat: %.10lf s\n",bw_it->numa_id, bw_it->mem_type, bw_it->avg_bw, bw_it->read_bw, bw_it->write_bw, bw_it->avg_lat, bw_it->read_lat, bw_it->write_lat);
		bw_it = bw_it->next;
	}
	fclose(conf);
}

int main(int argc, char ** argv){
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
	size_t size = 512*1024*1024;
	int *a;
	clock_t start, end;
	if(argc == 1){
		printf("Enter memory technologies available in ascending order of speed. eg: GPU NVRAM DRAM HBM\n");
		return -1;
	}
	else{
		mem_types = argc - 1;
		mem_tech = (char**)malloc(argc*sizeof(char*));
		for(int a = 1; a < argc; a++){
			mem_tech[a-1] = argv[a];
//			printf("%s\n",mem_tech[a-1]);
		}

	}
  	i = 0;
	while(i < total_numa_nodes){
		int k = 0;
		double wbw_avg=0.0;
		double rbw_avg=0.0;
		double wlat_avg=0.0;
		double rlat_avg=0.0;
		for(k = 0; k < 30; k++){
			//printf("Device %d: ", numa_node_ids[i]);
			a = (int*)numa_alloc_onnode(size, numa_node_ids[i]);
			int j = 0;


			start = clock();
            		while(j < (size/sizeof(int))){
				j++;
			}
			end = clock();

            		clock_t empty = end - start;


            		j = 0;
			start = clock();
			while(j < (size/sizeof(int))){
				a[j] = 1;
				j++;
			}
			end = clock();
			wbw_avg += 512/((double)(end - start - empty) / CLOCKS_PER_SEC);
			wlat_avg += ((double)(end - start - empty) / CLOCKS_PER_SEC);
			//printf("Write BW: %lf MB/s ", 512/((double)(end - start - empty) / CLOCKS_PER_SEC));
			j = 0;
			start = clock();
			while(j < (size/sizeof(int))){
                	        int t = a[j];
                	        j++;
                	}
			end = clock();
			rbw_avg += 512/((double)(end - start - empty) / CLOCKS_PER_SEC);
			rlat_avg += ((double)(end - start - empty) / CLOCKS_PER_SEC);
			//printf("Read BW: %lf MB/s\n", 512/((double)(end - start - empty) / CLOCKS_PER_SEC));
			numa_free(a, size);
		//	k++;
		}
		struct numa_node_bw * node_bw = (struct numa_node_bw *)malloc(sizeof(struct numa_node_bw));
		node_bw->numa_id = numa_node_ids[i];
		node_bw->write_bw = wbw_avg/30;
		node_bw->read_bw = rbw_avg/30;
		node_bw->write_lat = ((double)wlat_avg/(double)(size/sizeof(int)))/30;
		node_bw->read_lat = ((double)rlat_avg/(double)(size/sizeof(int)))/30;
		node_bw->avg_bw = (node_bw->write_bw + node_bw->read_bw)/2;
		node_bw->avg_lat = (node_bw->write_lat + node_bw->read_lat)/2;
		node_bw->next = NULL;
		if(numa_node_list == NULL){
			numa_node_list = node_bw;
			numa_list_head = numa_node_list;
		}
		else{
			sort_list(node_bw);
		}
		i++;
	}
	classify();
	write_config_file();
	printf("Max nodes: %d\n", max_node);
	printf("CPU Count: %d\n", cpu_count);
}
