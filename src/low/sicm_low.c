#include "sicm_low.h"

#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <math.h>
#include <numa.h>
#include <numaif.h>
#include <sched.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include <sys/mman.h>
// https://www.mail-archive.com/devel@lists.open-mpi.org/msg20403.html
#ifndef MAP_HUGE_SHIFT
#include <linux/mman.h>
#endif
#include "sicm_impl.h"
#include "detect_devices.h"

#ifdef HIP
#include <hip/hip_runtime.h>
#endif

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

char * sicm_device_tag_str(sicm_device_tag tag) {
  switch(tag) {
    case SICM_DRAM:
        return "SICM_DRAM";
    case SICM_KNL_HBM:
        return "SICM_KNL_HBM";
    case SICM_POWERPC_HBM:
        return "SICM_POWERPC_HBM";
    case SICM_OPTANE:
        return "SICM_OPTANE";
    case SICM_HIP:
        return "SICM_HIP";
    case INVALID_TAG:
        break;
  }
  return NULL;
}

static int sicm_device_compare(const void * lhs, const void * rhs) {
  sicm_device * l = * (sicm_device **) lhs;
  sicm_device * r = * (sicm_device **) rhs;

  if (l->node != r->node) {
    return l->node - r->node;
  }

  if (l->page_size != r->page_size) {
      return l->page_size - r->page_size;
  }

  return l->tag - r->tag;
}

/* Only initialize SICM once */
static int sicm_init_count = 0;
static pthread_mutex_t sicm_init_count_mutex = PTHREAD_MUTEX_INITIALIZER;
static sicm_device_list sicm_global_devices = {};
static sicm_device *sicm_global_device_array = NULL;

/* set in sicm_init */
struct sicm_device *sicm_default_device_ptr = NULL;

struct sicm_device_list sicm_init() {
  /* Check whether or not the global devices list has been initialized already */
  pthread_mutex_lock(&sicm_init_count_mutex);
  if (sicm_init_count) {
      sicm_init_count++;
      pthread_mutex_unlock(&sicm_init_count_mutex);
      return sicm_global_devices;
  }

  // Find the number of huge page sizes
  int huge_page_size_count = 0;

  DIR* dir = opendir("/sys/kernel/mm/hugepages");
  struct dirent* entry = NULL;
  while((entry = readdir(dir)) != NULL)
    if(entry->d_name[0] != '.') huge_page_size_count++;

  int* huge_page_sizes = malloc(huge_page_size_count * sizeof(int));

  normal_page_size = numa_pagesize() / 1024;

  // Find the actual set of huge page sizes (reported in KiB)
  rewinddir(dir);
  int i = 0;
  while((entry = readdir(dir)) != NULL) {
    if(entry->d_name[0] != '.') {
      huge_page_sizes[i] = 0;
      int j;
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

  const int node_count = get_node_count();
  const int device_count = node_count * (huge_page_size_count + 1);
  sicm_global_device_array = malloc(device_count * sizeof(struct sicm_device));

  // initialize the device list
  sicm_device **devices = malloc(device_count * sizeof(sicm_device *));
  for(int i = 0; i < device_count; i++) {
      devices[i] = &sicm_global_device_array[i];
      devices[i]->tag = INVALID_TAG;
      devices[i]->node = -1;
      devices[i]->page_size = -1;
  }

  const int idx = detect_devices(node_count,
                                 huge_page_sizes, huge_page_size_count,
                                 normal_page_size,
                                 devices);

  free(huge_page_sizes);

  qsort(devices, idx, sizeof(sicm_device *), sicm_device_compare);

  sicm_global_devices = (struct sicm_device_list){ .count = idx, .devices = devices };

  sicm_default_device(0);

  sicm_init_count++;

  pthread_mutex_unlock(&sicm_init_count_mutex);
  return sicm_global_devices;
}

sicm_device *sicm_default_device(const unsigned int idx) {
    if (idx < sicm_global_devices.count) {
        sicm_default_device_ptr = sicm_global_devices.devices[idx];
    }

    return sicm_default_device_ptr;
}

/* Frees memory up */
void sicm_fini() {
  pthread_mutex_lock(&sicm_init_count_mutex);
  if (sicm_init_count) {
      sicm_init_count--;
      if (sicm_init_count == 0) {
          free(sicm_global_devices.devices);
          free(sicm_global_device_array);
          memset(&sicm_global_devices, 0, sizeof(sicm_global_devices));
      }
  }
  pthread_mutex_unlock(&sicm_init_count_mutex);
}

void sicm_device_list_free(sicm_device_list *devs) {
  if (devs == NULL)
	return;

  free(devs->devices);
}

sicm_device *sicm_find_device(sicm_device_list *devs, const sicm_device_tag type, const int page_size, sicm_device *old) {
    sicm_device *dev = NULL;
    if (devs) {
      unsigned int i;
      for(i = 0; i < devs->count; i++) {
        if ((devs->devices[i]->tag == type) &&
            ((page_size == 0) || (sicm_device_page_size(devs->devices[i]) == page_size)) &&
            !sicm_device_eq(devs->devices[i], old)) {
          dev = devs->devices[i];
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
    case SICM_OPTANE:
    case SICM_POWERPC_HBM:
      ; // labels can't be followed by declarations
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
    case SICM_HIP:
    #ifdef HIP
        {
            // record previously selected device
            int old_dev = -1;
            if (hipGetDevice(&old_dev) != hipSuccess) {
                return NULL;
            }

            hipSetDevice(device->data.hip.id);

            void *ptr = NULL;
            hipHostMalloc(&ptr, size, 0);

            // restore previously selected device
            hipSetDevice(old_dev);
            return ptr;
        }
    #endif
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
    case SICM_OPTANE:
    case SICM_POWERPC_HBM:
      ; // labels can't be followed by declarations
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
    case SICM_HIP:
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
    case SICM_OPTANE:
    case SICM_POWERPC_HBM:
      return 1;
    case SICM_HIP:
    case INVALID_TAG:
      break;
  }
  return 0;
}

void* sicm_alloc_exact(struct sicm_device* device, void* base, size_t size) {
  switch(device->tag) {
    case SICM_DRAM:
    case SICM_KNL_HBM:
    case SICM_OPTANE:
    case SICM_POWERPC_HBM:
      ; // labels can't be followed by declarations
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
    case SICM_HIP:
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
    case SICM_OPTANE:
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
    case SICM_HIP:
        #ifdef HIP
        hipHostFree(ptr);
        #endif
        break;
    case INVALID_TAG:
    default:
      printf("error in sicm_device_free: unknown tag\n");
      exit(-1);
  }
}

int sicm_numa_id(struct sicm_device* device) {
    return device?device->node:-1;
}

int sicm_device_page_size(struct sicm_device* device) {
    return device?device->page_size:-1;
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

  if (dev1->node != dev2->node) {
      return 0;
  }

  if (dev1->page_size != dev2->page_size) {
      return 0;
  }

  switch(dev1->tag) {
    case SICM_DRAM:
      return 1;
    case SICM_KNL_HBM:
      return
          (dev1->data.knl_hbm.compute_node == dev2->data.knl_hbm.compute_node);
    case SICM_OPTANE:
      return
          (dev1->data.optane.compute_node == dev2->data.optane.compute_node);
    case SICM_POWERPC_HBM:
      return 1;
    case SICM_HIP:
      return (dev1->data.hip.id == dev2->data.hip.id);
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
    case SICM_KNL_HBM:
    case SICM_OPTANE:
    case SICM_POWERPC_HBM:
      #pragma omp parallel
      ret = numa_run_on_node(device->node);
      break;
    case SICM_HIP:
    case INVALID_TAG:
      break;
  }
  return ret;
}

/**
 * @input     buf - sting with meminfo data
 * @input buf_len - length of buffer (buf)
 * @input   field - field looking for (e.g., "MemFree")
 * @inout   value - output result found in buf input
 *
 * @return: -1 (error), 0 (not found), 1 (found)
 *
 * @Notes:
 *  - Note this assumes you do not split meminfo lines up,
 *    or at least the fields you care about are fully contained
 *    in the input buffer (i.e., not split up between reads and
 *    get partial line of input in buf).
 *  - Field names look like "MemTotal"
 *  - Not very pretty, but gets the correct values from meminfo
 *    likely needs some more bounds checking (e.g., buf[i]).
 */
static int parse_meminfo(char *buf, int buf_len, char *field, size_t *value)
{
    char str[128];
    int i;
    int found = 0;

    if (0 >= buf_len) {
        fprintf (stderr, "Error: Bad parameter (bugus buf_len)\n");
        return -1;
    }

    if ((NULL == buf) || (NULL == field) || (NULL == value)) {
        fprintf (stderr, "Error: Bad parameter\n");
        return -1;
    }

    for (i=0; i <= buf_len; i++) {
        if (buf[i] == field[0]) {
            char *s1 = &buf[i];
            char *s2 = &field[0];
            char tmp[128];
            int k=0;
            while (*s1++ == *s2++) {
                i++;
            }
            if (buf[i] == ':') {
                /* This is our line of info */

                /* Move past colon */
                i++;

                /* Move past blank spaces (careful of buf_len) */
                while ((i <= buf_len) && (buf[i] == ' ')) {
                    i++;
                }

                /*
                 * Grab digits before space and units, e.g.,
                 *    Node 0 MemFree:         6348756 kB
                 */
                while ((i <= buf_len) && (buf[i] != ' ')) {
                    tmp[k] = buf[i];
                    k++;
                    i++;
                }
                tmp[k] = '\0';

                *value = strtol(tmp, NULL, 0);

                /* Found, all done. */
                found = 1;
                break;
            }
            /* NOT our match, keep looking*/
        }
    }

    return found;
}

size_t sicm_capacity(struct sicm_device* device) {
  static const size_t path_len = 100;
  char path[path_len];
  int i;
  switch(device->tag) {
    case SICM_DRAM:
    case SICM_KNL_HBM:
    case SICM_OPTANE:
    case SICM_POWERPC_HBM:;
      int node = sicm_numa_id(device);
      int page_size = sicm_device_page_size(device);
      if(page_size == normal_page_size) {
        snprintf(path, path_len, "/sys/devices/system/node/node%d/meminfo", node);
        int fd = open(path, O_RDONLY);
#if 0
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
#else
        char data[128];
        if (read(fd, data, 128) != 128) {
            close(fd);
            return -1;
        }
        close(fd);
        size_t res = 0;
        int rc = 0;
        /* TODO: More testing */
        rc = parse_meminfo(data, 128, "MemTotal", &res);
        if (rc <= 0) {
            fprintf(stderr, "Error: failed to get available memory for node %d\n", node);
            return -1;
        }
        return res;
#endif
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
    case SICM_OPTANE:
    case SICM_POWERPC_HBM:;
      int node = sicm_numa_id(device);
      int page_size = sicm_device_page_size(device);
      if(page_size == normal_page_size) {
        snprintf(path, path_len, "/sys/devices/system/node/node%d/meminfo", node);
        int fd = open(path, O_RDONLY);
#if 0
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
#else
        char data[128];
        if (read(fd, data, 128) != 128) {
            close(fd);
            return -1;
        }
        close(fd);
        size_t res = 0;
        int rc = 0;
        /* TODO: More testing */
        rc = parse_meminfo(data, 128, "MemFree", &res);
        if (rc <= 0) {
            fprintf(stderr, "Error: failed to get available memory for node %d\n", node);
            return -1;
        }
#endif
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
    case SICM_OPTANE:
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

  dist = numa_distance(sicm_numa_id(device), numa_node_of_cpu(sched_getcpu()));
  switch(device->tag) {
    case SICM_DRAM:
      return dist == 10;
    case SICM_KNL_HBM:
      return dist == 31;
    case SICM_OPTANE:
      return dist == 17;
    case SICM_POWERPC_HBM:
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
