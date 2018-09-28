#include "sicm_low.h"

#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <math.h>
#include <numa.h>
#include <numaif.h>
#include <sched.h>
#include <stdio.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include <sys/mman.h>
#include <hwloc.h>
// https://www.mail-archive.com/devel@lists.open-mpi.org/msg20403.html
#if HWLOC_API_VERSION < 0x00010b00
#define HWLOC_OBJ_NUMANODE HWLOC_OBJ_NODE
#define HWLOC_OBJ_PACKAGE HWLOC_OBJ_SOCKET
#endif
#ifndef MAP_HUGE_SHIFT
#include <linux/mman.h>
#endif
#include "sicm_impl.h"

#define X86_CPUID_MODEL_MASK        (0xf<<4)
#define X86_CPUID_EXT_MODEL_MASK    (0xf<<16)

int normal_page_size = -1;

sicm_device_tag sicm_get_device_tag(char *env) {
	size_t max_chars;

	max_chars = 32;

	if(strncmp(env, "SICM_DRAM", max_chars) == 0) {
		return SICM_DRAM;
	} else if(strncmp(env, "SICM_KNL_HBM", max_chars) == 0) {
		return SICM_KNL_HBM;
	} else if(strncmp(env, "SICM_POWERPC_HBM", max_chars) == 0) {
		return SICM_POWERPC_HBM;
  }

  return INVALID_TAG;
}

const char * const sicm_device_tag_str(sicm_device_tag tag) {
  switch(tag) {
    case SICM_DRAM:
        return "SICM_DRAM";
    case SICM_KNL_HBM:
        return "SICM_KNL_HBM";
    case SICM_POWERPC_HBM:
        return "SICM_POWERPC_HBM";
    case INVALID_TAG:
        break;
  }
  return NULL;
}

/* Only initialize SICM once */
static int sicm_init_count = 0;
static pthread_mutex_t sicm_init_count_mutex = PTHREAD_MUTEX_INITIALIZER;
static sicm_device_list sicm_global_devices = {};

struct sicm_device_list sicm_init() {
  /* Check whether or not the global devices list has been initialized already */
  pthread_mutex_lock(&sicm_init_count_mutex);
  if (sicm_init_count) {
      sicm_init_count++;
      pthread_mutex_unlock(&sicm_init_count_mutex);
      return sicm_global_devices;
  }

  hwloc_topology_t topology;
  hwloc_obj_t node, retnode;
  int node_count = numa_max_node() + 1, depth;
  normal_page_size = getpagesize() / 1024;
  unsigned n;

  /* Test using hwloc to get the entire hardware graph */
  hwloc_topology_init(&topology);
  hwloc_topology_load(topology);
  n = hwloc_get_nbobjs_by_type(topology, HWLOC_OBJ_NUMANODE);
  printf("Found %u memory devices on NUMA nodes:\n", n);

  node = NULL;
  depth = hwloc_get_type_depth(topology, HWLOC_OBJ_NUMANODE);
  while(node = hwloc_get_next_obj_by_depth(topology, depth, node)) {
    printf("Node: %p\n", node);
    printf("  Memory: %lu\n", node->memory.total_memory);
  }

  // Find the number of huge page sizes
  int huge_page_size_count = 0;
  DIR* dir;
  struct dirent* entry;
  dir = opendir("/sys/kernel/mm/hugepages");
  while((entry = readdir(dir)) != NULL)
    if(entry->d_name[0] != '.') huge_page_size_count++;
  closedir(dir);

  int device_count = node_count * (huge_page_size_count + 1);

  struct bitmask* non_dram_nodes = numa_bitmask_alloc(node_count);

  struct sicm_device* devices = malloc(device_count * sizeof(struct sicm_device));
  int* huge_page_sizes = malloc(huge_page_size_count * sizeof(int));

  int i, j;
  int idx = 0;

  // Find the actual set of huge page sizes (reported in KiB)
  dir = opendir("/sys/kernel/mm/hugepages");
  i = 0;
  while((entry = readdir(dir)) != NULL) {
    if(entry->d_name[0] != '.') {
      huge_page_sizes[i] = 0;
      for(j = 0; j < 10; j++) {
        if(entry->d_name[j] == '\0') {
          j = -1;
          break;
        }
      }
      if(j < 0) break;
      for(; entry->d_name[j] >= '0' && entry->d_name[j] <= '9'; j++) {
        huge_page_sizes[i] *= 10;
        huge_page_sizes[i] += entry->d_name[j] - '0';
      }
      i++;
    }
  }
  closedir(dir);

  struct bitmask* cpumask = numa_allocate_cpumask();
  int cpu_count = numa_num_possible_cpus();
  struct bitmask* compute_nodes = numa_bitmask_alloc(node_count);
  printf("Bitmask size: %lu\n", cpumask->size);
  i = 0;
  for(i = 0; i < node_count; i++) {
    numa_node_to_cpus(i, cpumask);
    for(j = 0; j < cpu_count; j++) {
      if(numa_bitmask_isbitset(cpumask, j)) {
        numa_bitmask_setbit(compute_nodes, i);
        break;
      }
    }
  }
  numa_free_cpumask(cpumask);

  #ifdef __x86_64__
  // Knights Landing
  uint32_t xeon_phi_model = (0x7<<4);
  uint32_t xeon_phi_ext_model = (0x5<<16);
  uint32_t registers[4];
  uint32_t expected = xeon_phi_model | xeon_phi_ext_model;
  asm volatile("cpuid":"=a"(registers[0]),
                       "=b"(registers[1]),
                       "=c"(registers[2]),
                       "=d"(registers[2]):"0"(1), "2"(0));
  uint32_t actual = registers[0] & (X86_CPUID_MODEL_MASK | X86_CPUID_EXT_MODEL_MASK);

  if (actual == expected) {
    for(i = 0; i <= numa_max_node(); i++) {
      if(!numa_bitmask_isbitset(compute_nodes, i)) {
        long size = -1;
        if ((numa_node_size(i, &size) != -1) && size) {
          int compute_node = -1;
          /*
           * On Knights Landing machines, high-bandwidth memory always has
           * higher NUMA distances (to prevent malloc from giving you HBM)
           * but I'm pretty sure the compute node closest to an HBM node
           * always has NUMA distance 31, e.g.,
           * https://goparallel.sourceforge.net/wp-content/uploads/2016/05/Colfax_KNL_Clustering_Modes_Guide.pdf
           */
          for(j = 0; j < numa_max_node(); j++) {
              if(numa_distance(i, j) == 31) compute_node = j;
          }
          devices[idx].tag = SICM_KNL_HBM;
          devices[idx].data.knl_hbm = (struct sicm_knl_hbm_data){ .node=i,
            .compute_node=compute_node, .page_size=normal_page_size };
          numa_bitmask_setbit(non_dram_nodes, i);
          idx++;
          for(j = 0; j < huge_page_size_count; j++) {
              devices[idx].tag = SICM_KNL_HBM;
              devices[idx].data.knl_hbm = (struct sicm_knl_hbm_data){ .node=i,
                .compute_node=compute_node, .page_size=huge_page_sizes[j] };
              idx++;
          }
        }
      }
    }
  }
  #endif

  #ifdef __powerpc__
  // Power PC
  for(i = 0; i <= numa_max_node(); i++) {
    if(!numa_bitmask_isbitset(compute_nodes, i)) {
      // make sure the numa node has memory on it
      long size = -1;
      if ((numa_node_size(i, &size) != -1) && size) {
        devices[idx].tag = SICM_POWERPC_HBM;
        devices[idx].data.powerpc_hbm = (struct sicm_powerpc_hbm_data){ .node=i, .page_size=normal_page_size };
        numa_bitmask_setbit(non_dram_nodes, i);
        idx++;
        for(j = 0; j < huge_page_size_count; j++) {
          devices[idx].tag = SICM_POWERPC_HBM;
          devices[idx].data.powerpc_hbm = (struct sicm_powerpc_hbm_data){ .node=i, .page_size=huge_page_sizes[j] };
          idx++;
        }
      }
    }
  }
  #endif

  // DRAM
  for(i = 0; i <= numa_max_node(); i++) {
    if(!numa_bitmask_isbitset(non_dram_nodes, i)) {
      long size = -1;
      if ((numa_node_size(i, &size) != -1) && size) {
        devices[idx].tag = SICM_DRAM;
        devices[idx].data.dram = (struct sicm_dram_data){ .node=i, .page_size=normal_page_size };
        idx++;
        for(j = 0; j < huge_page_size_count; j++) {
          devices[idx].tag = SICM_DRAM;
          devices[idx].data.dram = (struct sicm_dram_data){ .node=i, .page_size=huge_page_sizes[j] };
          idx++;
        }
      }
    }
  }

  numa_bitmask_free(compute_nodes);
  numa_bitmask_free(non_dram_nodes);
  free(huge_page_sizes);

  sicm_global_devices = (struct sicm_device_list){ .count = device_count, .devices = devices };
  sicm_init_count++;
  pthread_mutex_unlock(&sicm_init_count_mutex);
  return sicm_global_devices;
}

/* Frees memory up */
void sicm_fini() {
  pthread_mutex_lock(&sicm_init_count_mutex);
  if (sicm_init_count) {
      sicm_init_count--;
      if (sicm_init_count == 0) {
          free(sicm_global_devices.devices);
          memset(&sicm_global_devices, 0, sizeof(sicm_global_devices));
      }
  }
  pthread_mutex_unlock(&sicm_init_count_mutex);
}

sicm_device *sicm_find_device(sicm_device_list *devs, const sicm_device_tag type, const int page_size, sicm_device *old) {
    sicm_device *dev = NULL;
    if (devs) {
      unsigned int i;
      for(i = 0; i < devs->count; i++) {
        if ((devs->devices[i].tag == type) &&
            ((page_size == 0) || (sicm_device_page_size(&devs->devices[i]) == page_size)) &&
            !sicm_device_eq(&devs->devices[i], old)) {
          dev = &devs->devices[i];
          break;
        }
      }
    }
    return dev;
}

void* sicm_device_alloc(struct sicm_device* device, size_t size) {
  switch(device->tag) {
    case SICM_DRAM:
    case SICM_KNL_HBM:
    case SICM_POWERPC_HBM:; // labels can't be followed by declarations
      int page_size = sicm_device_page_size(device);
      if(page_size == normal_page_size)
        return numa_alloc_onnode(size, sicm_numa_id(device));
      else {
        int shift = 10; // i.e., 1024
        int remaining = page_size;
        while(remaining > 1) {
          shift++;
          remaining >>= 1;
        }
        int old_mode;
        nodemask_t old_nodemask;
        get_mempolicy(&old_mode, old_nodemask.n, numa_max_node() + 2, NULL, 0);
        nodemask_t nodemask;
        nodemask_zero(&nodemask);
        nodemask_set_compat(&nodemask, sicm_numa_id(device));
        set_mempolicy(MPOL_BIND, nodemask.n, numa_max_node() + 2);
        void* ptr = mmap(NULL, size, PROT_READ | PROT_WRITE,
          MAP_PRIVATE | MAP_ANONYMOUS | MAP_HUGETLB | (shift << MAP_HUGE_SHIFT), -1, 0);
        if(ptr == MAP_FAILED) {
          printf("huge page allocation error: %s\n", strerror(errno));
        }
        set_mempolicy(old_mode, old_nodemask.n, numa_max_node() + 2);
        return ptr;
      }
    case INVALID_TAG:
      break;
  }
  printf("error in sicm_alloc: unknown tag\n");
  exit(-1);
}

void* sicm_device_alloc_mmapped(struct sicm_device* device, size_t size, int fd, off_t offset) {
  switch(device->tag) {
    case SICM_DRAM:
    case SICM_KNL_HBM:
    case SICM_POWERPC_HBM:; // labels can't be followed by declarations
      int page_size = sicm_device_page_size(device);
      if(page_size == normal_page_size)
        return numa_alloc_onnode(size, sicm_numa_id(device));
      else {
        int shift = 10; // i.e., 1024
        int remaining = page_size;
        while(remaining > 1) {
          shift++;
          remaining >>= 1;
        }
        int old_mode;
        nodemask_t old_nodemask;
        get_mempolicy(&old_mode, old_nodemask.n, numa_max_node() + 2, NULL, 0);
        nodemask_t nodemask;
        nodemask_zero(&nodemask);
        nodemask_set_compat(&nodemask, sicm_numa_id(device));
        set_mempolicy(MPOL_BIND, nodemask.n, numa_max_node() + 2);
        void* ptr = mmap(NULL, size, PROT_READ | PROT_WRITE,
          MAP_SHARED, fd, offset);
        if(ptr == MAP_FAILED) {
          printf("huge page allocation error: %s\n", strerror(errno));
        }
        set_mempolicy(old_mode, old_nodemask.n, numa_max_node() + 2);
        return ptr;
      }
    case INVALID_TAG:
      break;
  }
  printf("error in sicm_alloc: unknown tag\n");
  exit(-1);
}

int sicm_can_place_exact(struct sicm_device* device) {
  switch(device->tag) {
    case SICM_DRAM:
    case SICM_KNL_HBM:
    case SICM_POWERPC_HBM:
      return 1;
    case INVALID_TAG:
      break;
  }
  return 0;
}

void* sicm_alloc_exact(struct sicm_device* device, void* base, size_t size) {
  switch(device->tag) {
    case SICM_DRAM:
    case SICM_KNL_HBM:
    case SICM_POWERPC_HBM:; // labels can't be followed by declarations
      int page_size = sicm_device_page_size(device);
      if(page_size == normal_page_size) {
        int old_mode;
        nodemask_t old_nodemask;
        get_mempolicy(&old_mode, old_nodemask.n, numa_max_node() + 2, NULL, 0);
        nodemask_t nodemask;
        nodemask_zero(&nodemask);
        nodemask_set_compat(&nodemask, sicm_numa_id(device));
        set_mempolicy(MPOL_BIND, nodemask.n, numa_max_node() + 2);
        void* ptr = mmap(base, size, PROT_READ | PROT_WRITE,
          MAP_PRIVATE | MAP_FIXED | MAP_ANONYMOUS, -1, 0);
        if(ptr == (void*)-1) {
          printf("exact allocation error: %s\n", strerror(errno));
        }
        set_mempolicy(old_mode, old_nodemask.n, numa_max_node() + 2);
        return ptr;
      }
      else {
        int shift = 10; // i.e., 1024
        int remaining = page_size;
        while(remaining > 1) {
          shift++;
          remaining >>= 1;
        }
        int old_mode;
        nodemask_t old_nodemask;
        get_mempolicy(&old_mode, old_nodemask.n, numa_max_node() + 2, NULL, 0);
        nodemask_t nodemask;
        nodemask_zero(&nodemask);
        nodemask_set_compat(&nodemask, sicm_numa_id(device));
        set_mempolicy(MPOL_BIND, nodemask.n, numa_max_node() + 2);
        void* ptr = mmap(base, size, PROT_READ | PROT_WRITE,
          MAP_PRIVATE | MAP_FIXED | MAP_ANONYMOUS | MAP_HUGETLB | (shift << MAP_HUGE_SHIFT), -1, 0);
        printf("alloc exact: %p, %p\n", base, ptr);
        if(ptr == (void*)-1) {
          printf("huge page allocation error: %s\n", strerror(errno));
        }
        set_mempolicy(old_mode, old_nodemask.n, numa_max_node() + 2);
        return ptr;
      }
    case INVALID_TAG:
      break;
  }
  printf("error in sicm_alloc_exact: unknown tag\n");
  exit(-1);
}

void sicm_device_free(struct sicm_device* device, void* ptr, size_t size) {
  switch(device->tag) {
    case SICM_DRAM:
    case SICM_KNL_HBM:
    case SICM_POWERPC_HBM:
      if(sicm_device_page_size(device) == normal_page_size)
        //numa_free(ptr, size);
        munmap(ptr, size);
      else {
        // Huge page allocation occurs in whole page chunks, so we need
        // to free (unmap) in whole page chunks.
        int page_size = sicm_device_page_size(device);
        munmap(ptr, sicm_div_ceil(size, page_size * 1024) * page_size * 1024);
      }
      break;
    case INVALID_TAG:
    default:
      printf("error in sicm_device_free: unknown tag\n");
      exit(-1);
  }
}

int sicm_numa_id(struct sicm_device* device) {
  switch(device->tag) {
    case SICM_DRAM:
      return device->data.dram.node;
    case SICM_KNL_HBM:
      return device->data.knl_hbm.node;
    case SICM_POWERPC_HBM:
      return device->data.powerpc_hbm.node;
    case INVALID_TAG:
    default:
      return -1;
  }
}

int sicm_device_page_size(struct sicm_device* device) {
  switch(device->tag) {
    case SICM_DRAM:
      return device->data.dram.page_size;
    case SICM_KNL_HBM:
      return device->data.knl_hbm.page_size;
    case SICM_POWERPC_HBM:
      return device->data.powerpc_hbm.page_size;
    case INVALID_TAG:
    default:
      return -1;
  }
}

int sicm_device_eq(sicm_device* dev1, sicm_device* dev2) {
  if (!dev1 || !dev2) {
    return 0;
  }

  if (dev1 == dev2) {
    return 1;
  }

  if (dev1->tag != dev2->tag) {
      return 0;
  }

  switch(dev1->tag) {
    case SICM_DRAM:
      return
          (dev1->data.dram.node == dev2->data.dram.node) &&
          (dev1->data.dram.page_size == dev2->data.dram.page_size);
    case SICM_KNL_HBM:
      return
          (dev1->data.knl_hbm.node == dev2->data.knl_hbm.node) &&
          (dev1->data.knl_hbm.compute_node == dev2->data.knl_hbm.compute_node) &&
          (dev1->data.knl_hbm.page_size == dev2->data.knl_hbm.page_size);
    case SICM_POWERPC_HBM:
      return
          (dev1->data.powerpc_hbm.node == dev2->data.powerpc_hbm.node) &&
          (dev1->data.powerpc_hbm.page_size == dev2->data.powerpc_hbm.page_size);
    case INVALID_TAG:
    default:
      return 0;
  }

  return 0;
}

int sicm_move(struct sicm_device* src, struct sicm_device* dst, void* ptr, size_t size) {
  if(sicm_numa_id(src) >= 0) {
    int dst_node = sicm_numa_id(dst);
    if(dst_node >= 0) {
      nodemask_t nodemask;
      nodemask_zero(&nodemask);
      nodemask_set_compat(&nodemask, dst_node);
      return mbind(ptr, size, MPOL_BIND, nodemask.n, numa_max_node() + 2, MPOL_MF_MOVE);
    }
  }
  return -1;
}

int sicm_pin(struct sicm_device* device) {
  int ret = -1;
  switch(device->tag) {
    case SICM_DRAM:
      #pragma omp parallel
      ret = numa_run_on_node(device->data.dram.node);
      break;
    case SICM_KNL_HBM:
      #pragma omp parallel
      ret = numa_run_on_node(device->data.knl_hbm.compute_node);
      break;
    case SICM_POWERPC_HBM:
      #pragma omp parallel
      ret = numa_run_on_node(device->data.powerpc_hbm.node);
      break;
    case INVALID_TAG:
      break;
  }
  return ret;
}

size_t sicm_capacity(struct sicm_device* device) {
  static const size_t path_len = 100;
  char path[path_len];
  int i;
  switch(device->tag) {
    case SICM_DRAM:
    case SICM_KNL_HBM:
    case SICM_POWERPC_HBM:;
      int node = sicm_numa_id(device);
      int page_size = sicm_device_page_size(device);
      if(page_size == normal_page_size) {
        snprintf(path, path_len, "/sys/devices/system/node/node%d/meminfo", node);
        int fd = open(path, O_RDONLY);
        char data[31];
        if (read(fd, data, 31) != 31) {
            close(fd);
            return -1;
        }
        close(fd);
        size_t res = 0;
        size_t factor = 1;
        for(i = 30; data[i] != ' '; i--) {
          res += factor * (data[i] - '0');
          factor *= 10;
        }
        return res;
      }
      else {
        snprintf(path, path_len, "/sys/devices/system/node/node%d/hugepages/hugepages-%dkB/nr_hugepages", node, page_size);
        int fd = open(path, O_RDONLY);
        int pages = 0;
        char data[10];
        while(read(fd, data, 10) > 0) {
          for(i = 0; i < 10; i++) {
            if(data[i] < '0' || data[i] > '9') break;
            pages *= 10;
            pages += data[i] - '0';
          }
        }
        close(fd);
        return pages * page_size;
      }
    case INVALID_TAG:
    default:
      return -1;
  }
}

size_t sicm_avail(struct sicm_device* device) {
  static const size_t path_len = 100;
  char path[path_len];
  int i;
  switch(device->tag) {
    case SICM_DRAM:
    case SICM_KNL_HBM:
    case SICM_POWERPC_HBM:;
      int node = sicm_numa_id(device);
      int page_size = sicm_device_page_size(device);
      if(page_size == normal_page_size) {
        snprintf(path, path_len, "/sys/devices/system/node/node%d/meminfo", node);
        int fd = open(path, O_RDONLY);
        char data[66];
        if (read(fd, data, 66) != 66) {
            close(fd);
            return -1;
        }
        close(fd);
        size_t res = 0;
        size_t factor = 1;
        for(i = 65; data[i] != ' '; i--) {
          res += factor * (data[i] - '0');
          factor *= 10;
        }
        return res;
      }
      else {
        snprintf(path, path_len, "/sys/devices/system/node/node%d/hugepages/hugepages-%dkB/free_hugepages", node, page_size);
        int fd = open(path, O_RDONLY);
        int pages = 0;
        char data[10];
        while(read(fd, data, 10) > 0) {
          for(i = 0; i < 10; i++) {
            if(data[i] < '0' || data[i] > '9') break;
            pages *= 10;
            pages += data[i] - '0';
          }
        }
        close(fd);
        return pages * page_size;
      }
    case INVALID_TAG:
    default:
      return -1;
  }
}

int sicm_model_distance(struct sicm_device* device) {
  switch(device->tag) {
    case SICM_DRAM:
    case SICM_KNL_HBM:
    case SICM_POWERPC_HBM:;
      int node = sicm_numa_id(device);
      return numa_distance(node, numa_node_of_cpu(sched_getcpu()));
    case INVALID_TAG:
    default:
      return -1;
  }
}

int sicm_is_near(struct sicm_device* device) {
  int dist;
  switch(device->tag) {
    case SICM_DRAM:
      dist = numa_distance(sicm_numa_id(device), numa_node_of_cpu(sched_getcpu()));
      return dist == 10;
    case SICM_KNL_HBM:
      dist = numa_distance(sicm_numa_id(device), numa_node_of_cpu(sched_getcpu()));
      return dist == 31;
    case SICM_POWERPC_HBM:
      dist = numa_distance(sicm_numa_id(device), numa_node_of_cpu(sched_getcpu()));
      return dist == 80;
    case INVALID_TAG:
    default:
      return 0;
  }
}

void sicm_latency(struct sicm_device* device, size_t size, int iter, struct sicm_timing* res) {
  struct timespec start, end;
  int i;
  char b = 0;
  unsigned int n = time(NULL);
  clock_gettime(CLOCK_MONOTONIC_RAW, &start);
  char* blob = sicm_device_alloc(device, size);
  clock_gettime(CLOCK_MONOTONIC_RAW, &end);
  res->alloc = (end.tv_sec - start.tv_sec) * 1000000 + (end.tv_nsec - start.tv_nsec) / 1000;
  clock_gettime(CLOCK_MONOTONIC_RAW, &start);
  for(i = 0; i < iter; i++) {
    sicm_rand(n);
    blob[n % size] = 0;
  }
  clock_gettime(CLOCK_MONOTONIC_RAW, &end);
  res->write = (end.tv_sec - start.tv_sec) * 1000000 + (end.tv_nsec - start.tv_nsec) / 1000;
  clock_gettime(CLOCK_MONOTONIC_RAW, &start);
  for(i = 0; i < iter; i++) {
    sicm_rand(n);
    b = blob[n % size];
  }
  clock_gettime(CLOCK_MONOTONIC_RAW, &end);
  // Write it back so hopefully it won't compile away the read
  blob[0] = b;
  res->read = (end.tv_sec - start.tv_sec) * 1000000 + (end.tv_nsec - start.tv_nsec) / 1000;
  clock_gettime(CLOCK_MONOTONIC_RAW, &start);
  sicm_device_free(device, blob, size);
  clock_gettime(CLOCK_MONOTONIC_RAW, &end);
  res->free = (end.tv_sec - start.tv_sec) * 1000000 + (end.tv_nsec - start.tv_nsec) / 1000;
}

size_t sicm_bandwidth_linear2(struct sicm_device* device, size_t size,
    size_t (*kernel)(double*, double*, size_t)) {
  struct timespec start, end;
  double* a = sicm_device_alloc(device, size * sizeof(double));
  double* b = sicm_device_alloc(device, size * sizeof(double));
  unsigned int i;
  #pragma omp parallel for
  for(i = 0; i < size; i++) {
    a[i] = 1;
    b[i] = 2;
  }
  clock_gettime(CLOCK_MONOTONIC_RAW, &start);
  size_t accesses = kernel(a, b, size);
  clock_gettime(CLOCK_MONOTONIC_RAW, &end);
  size_t delta = (end.tv_sec - start.tv_sec) * 1000000 + (end.tv_nsec - start.tv_nsec) / 1000;
  sicm_device_free(device, a, size * sizeof(double));
  sicm_device_free(device, b, size * sizeof(double));
  return accesses / delta;
}

size_t sicm_bandwidth_random2(struct sicm_device* device, size_t size,
    size_t (*kernel)(double*, double*, size_t*, size_t)) {
  struct timespec start, end;
  double* a = sicm_device_alloc(device, size * sizeof(double));
  double* b = sicm_device_alloc(device, size * sizeof(double));
  size_t* indexes = sicm_device_alloc(device, size * sizeof(size_t));
  unsigned int i;
  #pragma omp parallel for
  for(i = 0; i < size; i++) {
    a[i] = 1;
    b[i] = 2;
    indexes[i] = sicm_hash(i) % size;
  }
  clock_gettime(CLOCK_MONOTONIC_RAW, &start);
  size_t accesses = kernel(a, b, indexes, size);
  clock_gettime(CLOCK_MONOTONIC_RAW, &end);
  size_t delta = (end.tv_sec - start.tv_sec) * 1000000 + (end.tv_nsec - start.tv_nsec) / 1000;
  sicm_device_free(device, a, size * sizeof(double));
  sicm_device_free(device, b, size * sizeof(double));
  sicm_device_free(device, indexes, size * sizeof(size_t));
  return accesses / delta;
}

size_t sicm_bandwidth_linear3(struct sicm_device* device, size_t size,
    size_t (*kernel)(double*, double*, double*, size_t)) {
  struct timespec start, end;
  double* a = sicm_device_alloc(device, 3 * size * sizeof(double));
  double* b = &a[size];
  double* c = &a[size * 2];
  unsigned int i;
  #pragma omp parallel for
  for(i = 0; i < size; i++) {
    a[i] = 1;
    b[i] = 2;
    c[i] = 3;
  }
  clock_gettime(CLOCK_MONOTONIC_RAW, &start);
  size_t accesses = kernel(a, b, c, size);
  clock_gettime(CLOCK_MONOTONIC_RAW, &end);
  size_t delta = (end.tv_sec - start.tv_sec) * 1000000 + (end.tv_nsec - start.tv_nsec) / 1000;
  sicm_device_free(device, a, 3 * size * sizeof(double));
  return accesses / delta;
}

size_t sicm_bandwidth_random3(struct sicm_device* device, size_t size,
    size_t (*kernel)(double*, double*, double*, size_t*, size_t)) {
  struct timespec start, end;
  double* a = sicm_device_alloc(device, size * sizeof(double));
  double* b = sicm_device_alloc(device, size * sizeof(double));
  double* c = sicm_device_alloc(device, size * sizeof(double));
  size_t* indexes = sicm_device_alloc(device, size * sizeof(size_t));
  unsigned int i;
  #pragma omp parallel for
  for(i = 0; i < size; i++) {
    a[i] = 1;
    b[i] = 2;
    c[i] = 3;
    indexes[i] = sicm_hash(i) % size;
  }
  clock_gettime(CLOCK_MONOTONIC_RAW, &start);
  size_t accesses = kernel(a, b, c, indexes, size);
  clock_gettime(CLOCK_MONOTONIC_RAW, &end);
  size_t delta = (end.tv_sec - start.tv_sec) * 1000000 + (end.tv_nsec - start.tv_nsec) / 1000;
  sicm_device_free(device, a, size * sizeof(double));
  sicm_device_free(device, b, size * sizeof(double));
  sicm_device_free(device, c, size * sizeof(double));
  sicm_device_free(device, indexes, size * sizeof(size_t));
  return accesses / delta;
}

size_t sicm_triad_kernel_linear(double* a, double* b, double* c, size_t size) {
  int i;
  double scalar = 3.0;
  #pragma omp parallel for
  for(i = 0; i < size; i++) {
    a[i] = b[i] + scalar * c[i];
  }
  return size * 3 * sizeof(double);
}

size_t sicm_triad_kernel_random(double* a, double* b, double* c, size_t* indexes, size_t size) {
  int i, idx;
  double scalar = 3.0;
  #pragma omp parallel for
  for(i = 0; i < size; i++) {
    idx = indexes[i];
    a[idx] = b[idx] + scalar * c[idx];
  }
  return size * (sizeof(size_t) + 3 * sizeof(double));
}
