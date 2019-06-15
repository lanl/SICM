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
	double rw_bw;
	double ran_bw;
	double lin_bw;
	double read_lat;
	double write_lat;
	double rw_lat;
	double ran_lat;
	double lin_lat;
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
	int i;
        struct numa_node_bw * bw_it = numa_list_head;
	while(bw_it != NULL){
		i = 0;
		double delta = 999999999.9999999;
		while(i < mem_types){
			double dist = abs(sqrt(abs((means[i] - bw_it->avg_bw))*abs((means[i] - bw_it->avg_bw))));
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
				}
			}
			i++;
		}
		bw_it = bw_it->next;
	}
}

void calculate_mean(){
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
		i++;
	}
	calculate_distances();
}

void classify(){
	int cluster_size;
	int last_cluster_size;
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
		if((bw_it->avg_bw > new_node->avg_bw)){
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
	conf = fopen("sicm_numa_config", "w");
	struct numa_node_bw * bw_it = numa_list_head;
	while(bw_it != NULL){	
		fprintf(conf, "%d %s %lf %lf %lf %lf %lf %lf %.10lf %.10lf %.10lf %.10lf %.10lf %.10lf\n",bw_it->numa_id, bw_it->mem_type, bw_it->avg_bw, bw_it->read_bw, bw_it->write_bw, bw_it->rw_bw, bw_it->ran_bw, bw_it->lin_bw, bw_it->avg_lat, bw_it->read_lat, bw_it->write_lat, bw_it->rw_lat, bw_it->ran_lat, bw_it->lin_lat);
		printf("%d %s %lf %lf %lf %lf %lf %lf %.10lf %.10lf %.10lf %.10lf %.10lf %.10lf\n",bw_it->numa_id, bw_it->mem_type, bw_it->avg_bw, bw_it->read_bw, bw_it->write_bw, bw_it->rw_bw, bw_it->ran_bw, bw_it->lin_bw, bw_it->avg_lat, bw_it->read_lat, bw_it->write_lat, bw_it->rw_lat, bw_it->ran_lat, bw_it->lin_lat);
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
	srand(clock());
	sleep(10);
	if(argc == 1){
		printf("Enter memory technologies available in ascending order of speed. eg: GPU NVRAM DRAM HBM\n");
		return -1;
	}
	else{
		mem_types = argc - 1;
		mem_tech = (char**)malloc(argc*sizeof(char*));
		for(int a = 1; a < argc; a++){
			mem_tech[a-1] = argv[a];
		}

	}
  	i = 0;
	while(i < total_numa_nodes){
		int k = 0;
		double wbw_avg=0.0;
		double rbw_avg=0.0;
		double rwbw_avg=0.0;
		double ranbw_avg=0.0;
		double linbw_avg=0.0;
		double wlat_avg=0.0;
		double rlat_avg=0.0;
		double rwlat_avg=0.0;
		double ranlat_avg=0.0;
		double linlat_avg=0.0;
		for(k = 0; k < 30; k++){
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
			j = 0;
			start = clock();
			while(j < (size/sizeof(int))){
                	        int t = a[j];
                	        j++;
            }
			end = clock();
			rbw_avg += 512/((double)(end - start - empty) / CLOCKS_PER_SEC);
			rlat_avg += ((double)(end - start - empty) / CLOCKS_PER_SEC);
			j = 0;
			start = clock();
            while(j < (size/sizeof(int))){
                            a[j] += a[j];
                            j++;
            }
            end = clock();                                                                                     
            rwbw_avg += 512/((double)(end - start - empty) / CLOCKS_PER_SEC);
            rwlat_avg += ((double)(end - start - empty) / CLOCKS_PER_SEC);
			j = 0;
			start = clock();
            while(j < (size/sizeof(int))){
                            a[((int)rand())%((int)(size/sizeof(int)))] += a[((int)rand())%((int)(size/sizeof(int)))];
                            j++;
            }
            end = clock();                                                                                     
            ranbw_avg += 512/((double)(end - start - empty) / CLOCKS_PER_SEC);
            ranlat_avg += ((double)(end - start - empty) / CLOCKS_PER_SEC);
			j = 0;
			start = clock();
            while(j < (size/sizeof(int))){
                            a[(j+10)%((int)(size/sizeof(int)))] += a[(j+2j)%((int)(size/sizeof(int)))];
                            j++;
            }
            end = clock();                                                                                     
            linbw_avg += 512/((double)(end - start - empty) / CLOCKS_PER_SEC);
            linlat_avg += ((double)(end - start - empty) / CLOCKS_PER_SEC);
			numa_free(a, size);
		}
		struct numa_node_bw * node_bw = (struct numa_node_bw *)malloc(sizeof(struct numa_node_bw));
		node_bw->numa_id = numa_node_ids[i];
		node_bw->write_bw = wbw_avg/30;
		node_bw->read_bw = rbw_avg/30;
		node_bw->rw_bw = rwbw_avg/30;
		node_bw->ran_bw = ranbw_avg/30;
		node_bw->lin_bw = linbw_avg/30;
		node_bw->write_lat = ((double)wlat_avg/(double)(size/sizeof(int)))/30;
		node_bw->read_lat = ((double)rlat_avg/(double)(size/sizeof(int)))/30;
        node_bw->rw_lat = ((double)rwlat_avg/(double)(size/sizeof(int)))/30;
        node_bw->ran_lat = ((double)ranlat_avg/(double)(size/sizeof(int)))/30;
        node_bw->lin_lat = ((double)linlat_avg/(double)(size/sizeof(int)))/30;
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
