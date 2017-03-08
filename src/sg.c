#include "sg.h"
#include "sicm_low.h"
#include <numa.h>
#include <stdlib.h>

struct suballoc_t {
  void* ptr;
  size_t sz;
};

struct allocation_t {
  void* ptr;
  struct suballoc_t* suballocs;
};

struct alloc_table_t {
  size_t used, capacity;
  struct allocation_t* data;
};

struct alloc_table_t alloc_table;

struct sicm_device_list sg_performance_list;
struct sicm_device_list sg_capacity_list;

void add_allocation(void* ptr, struct suballoc_t* suballocs) {
  size_t k;
  if(100 * alloc_table.used / alloc_table.capacity >= 80) {
    struct allocation_t* old_data = alloc_table.data;
    size_t old_capacity = alloc_table.capacity;
    alloc_table.capacity *= 2;
    alloc_table.used = 0;
    alloc_table.data = malloc(alloc_table.capacity * sizeof(struct alloc_table_t));
    for(k = 0; k < old_capacity; k++)
      if(old_data[k].ptr != NULL)
        add_allocation(old_data[k].ptr, old_data[k].suballocs);
    free(old_data);
  }
  k = sicm_hash((size_t)ptr) % alloc_table.capacity;
  while(1) {
    if(alloc_table.data[k].ptr == NULL) {
      alloc_table.data[k].ptr = ptr;
      alloc_table.data[k].suballocs = suballocs;
      break;
    }
    k = (k + 1) % alloc_table.capacity;
  }
}

struct suballoc_t* get_suballocs(void* ptr) {
  size_t k = sicm_hash((size_t)ptr) % alloc_table.capacity;
  while(1) {
    if(alloc_table.data[k].ptr == ptr)
      return alloc_table.data[k].suballocs;
    if(alloc_table.data[k].ptr == NULL) return NULL;
    k = (k + 1) % alloc_table.capacity;
  }
}

void remove_allocation(void* ptr) {
  size_t k = sicm_hash((size_t)ptr) % alloc_table.capacity;
  while(1) {
    if(alloc_table.data[k].ptr == ptr) {
      alloc_table.data[k].ptr = NULL;
      free(alloc_table.data[k].suballocs);
      alloc_table.data[k].suballocs = NULL;
      break;
    }
    if(alloc_table.data[k].ptr == NULL) break;
    k = (k + 1) % alloc_table.capacity;
  }
}

int compare_perf(struct sicm_device* a, struct sicm_device* b) {
  int a_near = sicm_is_near(a);
  int b_near = sicm_is_near(b);
  if(a_near && !b_near) return -1;
  if(!a_near && b_near) return 1;
  if(a_near) {
    if(a->tag == SICM_KNL_HBM && b->tag != SICM_KNL_HBM) return -1;
    if(a->tag == SICM_KNL_HBM) { // b is also KNL HBM
      if(a->data.knl_hbm.page_size > b->data.knl_hbm.page_size) return -1;
      return 1;
    }
    if(b->tag == SICM_KNL_HBM) return 1; // a is not KNL HBM
    // at this point a and b are not KNL HBM
    if(a->data.dram.page_size > b->data.dram.page_size) return -1;
    return 1;
  }
  else {
    // If we have to go to a far node, we want reverse preferences (i.e., DO NOT
    // allocate on a performant far node)
    if(a->tag == SICM_DRAM && b->tag != SICM_DRAM) return -1;
    if(a->tag == SICM_DRAM) { // b is also KNL HBM
      if(a->data.dram.page_size > b->data.dram.page_size) return -1;
      return 1;
    }
    if(b->tag == SICM_DRAM) return 1; // a is not KNL HBM
    // at this point a and b are not KNL HBM
    if(a->data.knl_hbm.page_size > b->data.knl_hbm.page_size) return -1;
    return 1;
  }
  return 0;
}

int compare_cap(struct sicm_device* a, struct sicm_device* b) {
  int a_near = sicm_is_near(a);
  int b_near = sicm_is_near(b);
  if(a_near && !b_near) return -1;
  if(!a_near && b_near) return 1;
  if(a->tag == SICM_DRAM && b->tag != SICM_DRAM) return -1;
  if(a->tag == SICM_DRAM) { // b is also KNL HBM
    if(a->data.dram.page_size > b->data.dram.page_size) return -1;
    return 1;
  }
  if(b->tag == SICM_DRAM) return 1; // a is not KNL HBM
  // at this point a and b are not KNL HBM
  if(a->data.knl_hbm.page_size > b->data.knl_hbm.page_size) return -1;
  return 1;
}

void sort_list(struct sicm_device_list* list, int (*cmp)(struct sicm_device*, struct sicm_device*)) {
  // This is the iterative version, so we need an explicit stack.
  int* stack = malloc(list->count * sizeof(int));
  int top = -1;
  stack[++top] = 0;
  stack[++top] = list->count - 1;
  int h, l;
  while(top >= 0) {
    h = stack[top--];
    l = stack[top--];

    // Partition the list and move the pivot to the right place
    // The pivot is list->devices[h]
    struct sicm_device swap;
    int i = l - 1;
    int j;
    for(j = l; j < h; j++) {
      if(cmp(&list->devices[j], &list->devices[h]) == -1) {
        i++;
        swap = list->devices[i];
        list->devices[i] = list->devices[j];
        list->devices[j] = swap;
      }
    }
    swap = list->devices[i+1];
    list->devices[i+1] = list->devices[h];
    list->devices[h] = swap;

    // Set up the "recursive call"
    // The pivot is now at location i + 1
    // Check if there are devices left of the pivot
    if(i > l) {
      stack[++top] = l;
      stack[++top] = i;
    }
    // Check if there are devices right of the pivot
    if(i+2 < h) {
      stack[++top] = i + 2;
      stack[++top] = h;
    }
  }
  free(stack);
}

void sg_init(int id) {
  int i, j;
  int node_count = numa_max_node() + 1;
  struct bitmask* cpumask = numa_allocate_cpumask();
  int cpu_count = numa_num_possible_cpus();
  int* compute_nodes = malloc(cpu_count * sizeof(int));
  int compute_node_count = 0;
  for(i = 0; i < node_count; i++) {
    numa_node_to_cpus(i, cpumask);
    for(j = 0; j < cpu_count; j++) {
      if(numa_bitmask_isbitset(cpumask, j)) {
        compute_nodes[compute_node_count] = j;
        compute_node_count++;
        break;
      }
    }
  }
  numa_free_cpumask(cpumask);
  #pragma omp parallel
  numa_run_on_node(compute_nodes[id % compute_node_count]);
  free(compute_nodes);

  sg_performance_list = sicm_init();
  sg_capacity_list = (struct sicm_device_list){
      .devices = malloc(sg_performance_list.count * sizeof(struct sicm_device)),
      .count = sg_performance_list.count
    };

  // Sort the performance list first, since that's an okay ordering for the
  // capacity list
  sort_list(&sg_performance_list, compare_perf);

  for(i = 0; i < sg_performance_list.count; i++)
    sg_capacity_list.devices[i] = sg_performance_list.devices[i];
  sort_list(&sg_capacity_list, compare_cap);
}
