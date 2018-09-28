/**
 * \file sicm_low.h
 * \brief Front end of the low-level interface.
 *
 * Defines most functions that users (e.g., high-level interface or
 * low-level application/run-time designers) should interact with.
 * Chiefly, this means initialization of the low-level data structures,
 * common abstractions over heterogeneous functions, and low-level
 * queries based on the common abstractions.
 */
#ifndef SICM_LOW_H
#define SICM_LOW_H

/// Prerequisite for sicm_model_distance.
#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif
#include <stdint.h>
#include <stdlib.h>
#include <sys/types.h>

// default arena
#define ARENA_DEFAULT NULL

/// Enumeration of supported memory device types.
/**
 * This tag can be used to identify a device, but its internal role is
 * to indicate which case of the sicm_device_data union is inhabited.
 */
typedef enum sicm_device_tag {
  SICM_DRAM,
  SICM_KNL_HBM,
  SICM_POWERPC_HBM,
  INVALID_TAG
} sicm_device_tag;

const char * const sicm_device_tag_str(sicm_device_tag tag);
sicm_device_tag sicm_get_device_tag(char *env);

/// Data specific to a DRAM device.
typedef struct sicm_dram_data {
  int node; ///< NUMA node
  int page_size; ///< Page size
} sicm_dram_data;

/// Data specific to a KNL HBM device.
typedef struct sicm_knl_hbm_data {
  int node; ///< NUMA node
  int compute_node;
  int page_size; ///< Page size
} sicm_knl_hbm_data;

/// Data specific to a PowerPC 9 HBM device.
typedef struct sicm_powerpc_hbm_data {
  int node;
  int page_size;
} sicm_powerpc_hbm_data;

/// Data that, given a device type, uniquely identify the device within that type.
/**
 * This union is only meaningful in the presence of a sicm_device_tag,
 * and together they identify a particular memory device. The
 * sicm_device_tag enumeration indicates which case of this union is
 * inhabited.
 */
typedef union sicm_device_data {
  sicm_dram_data dram;
  sicm_knl_hbm_data knl_hbm;
  sicm_powerpc_hbm_data powerpc_hbm;
} sicm_device_data;

/// Tagged/discriminated union that fully identifies a device.
/**
 * The combination of a sicm_device_tag and sicm_device_data identifies
 * a device. Given this, heterogeneous functions on memory devices
 * should switch on the tag, then use the data to further refine their
 * operations.
 */
typedef struct sicm_device {
  sicm_device_tag tag; ///< Type of memory device
  sicm_device_data data; ///< Per-type identifying information
} sicm_device;

/// Explicitly-sized sicm_device array.
typedef struct sicm_device_list {
  unsigned int count; ///< Number of devices in the array.
  sicm_device* devices; ///< Array of devices of count elements.
} sicm_device_list;

/// Results of a latency timing.
/**
 * All times are in milliseconds.
 */
struct sicm_timing {
  unsigned int alloc; ///< Time requried for allocation.
  unsigned int write; ///< Time required for writing.
  unsigned int read; ///< Time required for reading.
  unsigned int free; ///< Time required for deallocation.
};

/// Handle to an arena.
typedef void* sicm_arena;

/// Explicitly-sized sicm_arena list.
typedef struct sicm_arena_list {
	unsigned int count;
	sicm_arena *arenas;
} sicm_arena_list;

/// Initialize the low-level interface.
/**
 * Determine the total number of memory devices (which is the number of
 * NUMA nodes times the number of huge page sizes, plus the sum of all
 * non-NUMA memory devices such as CUDA GPUs) and allocates a
 * sicm_device_list. Per-device detection criteria are used to populate
 * the device list, which is then returned.
 */
sicm_device_list sicm_init();

/// Clean up the low-level interface.
/**
 * Frees up the devices list.
 */
void sicm_fini();

/// Find and return a device that matches the given type and page size
/**
 * @param devs the list of devices
 * @param type the type of device
 * @param page_size the size of the page; use 0 for any size
 * @param old a previously selected device that should not be considered for use
 * @return a pointer to a device that matches the given criteria or NULL
 */
sicm_device *sicm_find_device(sicm_device_list *devs, const sicm_device_tag type, const int page_size, sicm_device *old);

/// List of the defined arenas
/**
 * @return list of the defined arenas
 *
 * This function can be used to get the handles of all arenas that are
 * currently defined
 */
sicm_arena_list *sicm_arenas_list();

/// Create new arena
/**
 * @param maxsize maximum size of the arena.
 * @param dev initial device where the arena's allocations should use
 * @return handle to the newly created arena, or ARENA_DEFAULT if the
 *         the function failed.
 */
sicm_arena sicm_arena_create(size_t maxsize, sicm_device *dev);

/// Create new mapped arena
/**
 * @param maxsize maximum size of the arena.
 * @param dev initial device where the arena's allocations should use
 * @param fd A valid file descriptor to map the memory into
 * @param offset Starting offset within the file descriptor
 * @param mutex_fd A valid file descriptor to map the mutex into
 * @param mutex_offset Starting offset within the mutex file descriptor
 * @return handle to the newly created arena, or ARENA_DEFAULT if the
 *         the function failed.
 */
sicm_arena sicm_arena_create_mmapped(size_t maxsize, sicm_device *dev, int fd, off_t offset, int mutex_fd, off_t mutex_offset);

/// Free up arena
/**
 * @param handle to an arena you want to destroy
 */
void sicm_arena_destroy(sicm_arena arena);

/// Set default arena for the current thread
/**
 * @param sa arena to use when sicm_alloc is called. If the value is NULL,
 *           sa_alloc will use the standard (je_)malloc for the allocations.
 *
 */
void sicm_arena_set_default(sicm_arena sa);

/// Get default arena for the current thread
/**
 * @return the arena used for sa_alloc
 */
sicm_arena sicm_arena_get_default(void);

/// Get the NUMA node for an arena
/**
 * @param sa arena
 * @return device for the arena
 */
sicm_device *sicm_arena_get_device(sicm_arena sa);

/// Set the NUMA node for an arena
/**
 * @param sa arena
 * @param dev new device for the arena
 * @return zero if the operation is successful
 */
int sicm_arena_set_device(sicm_arena sa, sicm_device *dev);

/// Get arena size
/**
 * @param sa arena
 * @return arena size
 *
 * The returned value might be bigger than the sum of the sizes of the
 * allocated regions, because it is tracked at extent level and includes
 * the currently available free memory.
 */
size_t sicm_arena_size(sicm_arena sa);

/// Allocate memory region
/**
 * @param sa arena that should be used for the allocation. ARENA_DEFAULT is allowed.
 * @param sz size of the region
 * @return pointer to the new allocation, or NULL of the operation failed.
 *
 * Specifying ARENA_DEFAULT makes the function equivalent to malloc.
 */
void *sicm_arena_alloc(sicm_arena sa, size_t sz);

/// Allocate aligned memory region
/**
 * @param sa arena that should be used for the allocation. ARENA_DEFAULT is allowed.
 * @param sz size of the region
 * @param align the alignment of the address; must be a power of 2
 * @return pointer to the new allocation, or NULL of the operation failed.
 *
 * Specifying ARENA_DEFAULT makes the function equivalent to malloc.
 */
void *sicm_arena_alloc_aligned(sicm_arena sa, size_t sz, size_t align);

/// Resize a memory region in an arena
/**
 * @param sa arena that should be used for the allocation. ARENA_DEFAULT is allowed.
 * @param ptr pointer to the memory to be resized
 * @param sz new size
 * @return pointer to the new allocation, or NULL if unable to reallocate
 */
void *sicm_arena_realloc(sicm_arena sa, void *ptr, size_t sz);

/// Allocate memory region
/**
 * @param sz size of the region
 * @return pointer to the new allocation, or NULL if the operation failed.
 *
 * The function uses the default arena, if set. Otherwise it uses the standard
 * (je_)malloc function.
 */
void *sicm_alloc(size_t sz);

/// Allocate memory region
/**
 * @param sz size of the region
 * @param align the alignment of the address; must be a power of 2
 * @return pointer to the new allocation, or NULL if the operation failed.
 *
 * The function uses the default arena, if set. Otherwise it uses the standard
 * (je_)malloc function.
 */
void *sicm_alloc_aligned(size_t sz, size_t align);

/// Deallocate/free memory region
/**
 * @param ptr pointer to the memory to deallocated.
 */
void sicm_free(void *ptr);

/// Resize a memory region
/**
 * @param ptr pointer to the memory to be resized
 * @param sz new size
 * @return pointer to the new allocation, or NULL if unable to reallocate
 */
void *sicm_realloc(void *ptr, size_t sz);

/// Find out which arena a memory region belongs to
/**
 * @param ptr pointer to the memory region
 * @return handle to the arena, or ARENA_DEFAULT if unknown
 */
sicm_arena sicm_arena_lookup(void *ptr);

/// Allocate memory on a SICM device.
/**
 * @param[in] device Pointer to a sicm_device to allocate on.
 * @param[in] size Amount of memory to allocate.
 * @return Pointer to the start of the allocation.
 *
 * If you allocate on huge pages, your allocation will be rounded up to
 * a multiple of the huge page size. Also, if you try to allocate on
 * huge pages and there aren't enough huge pages available, the
 * allocation will fail and return -1.
 */
void* sicm_device_alloc(struct sicm_device* device, size_t size);

/// Returns whether the device supports exact placement.
/**
 * @param[in] device Pointer to a sicm_device to query.
 * @return Whether the device supports mapping with an exact base address.
 *
 * This indicates whether it's safe to call sicm_alloc_exact with the device in
 * question.
 */
int sicm_can_place_exact(struct sicm_device* device);

/// Allocate memory on a SICM device with an exact base address.
/**
 * @param[in] device Pointer to a sicm_device to allocate on.
 * @param[in] base Base address of the allocation.
 * @param[in] size Amount of memory to allocate.
 * @return Pointer to the start of the allocation.
 *
 * This function is incredibly unsafe: it'll either not work (if the device
 * doesn't support base addresses) or it might stomp previous allocations. Only
 * use this if you know what you're doing.
 */
void* sicm_device_alloc_exact(struct sicm_device* device, void* base, size_t size);

/// Allocate memory on a SICM device and mmap it to a file descriptor
/**
 * @param[in] device Pointer to a sicm_device to allocate on.
 * @param[in] size Amount of memory to allocate.
 * @param[in] fd A valid file descriptor to map the memory into
 * @param[in] offset Starting offset within the file descriptor
 * @return Pointer to the start of the allocation.
 */
void *sicm_device_alloc_mmapped(struct sicm_device* device, size_t size, int fd, off_t offset);

/// Deallocate/free memory on a SICM device.
/**
 * @param[in] device Pointer to the sicm_device the allocation was made on.
 * @param[in] ptr Pointer to the start of the allocation (i.e., return value of sicm_alloc).
 * @param[in] size Amount of memory to deallocate (should be the same as the allocation).
 */
void sicm_device_free(struct sicm_device* device, void* ptr, size_t size);

/// Get the NUMA node number of a SICM device.
/**
 * @param[in] device Pointer ot the sicm_device to query.
 * @return NUMA node number of the device. If the device is not a NUMA node, the return value is -1.
 */
int sicm_numa_id(sicm_device* device);

/// Get the page size of a SICM device.
/**
 * @param[in] device Pointer ot the sicm_device to query.
 * @return Page size of the device. If the device is not a NUMA node, the return value is -1.
 */
int sicm_device_page_size(sicm_device* device);

/// Compares devices for equality
/**
 * @param[in] dev1 the first device
 * @param[in] dev2 the second device
 * @return 1 if the devices are the same; 0 otherwise
 */
int sicm_device_eq(sicm_device* dev1, sicm_device* dev2);

/// Move data from one device to another.
/**
 * @param[in] src Device that currently contains the data.
 * @param[in] dst Device that ought to contain the data.
 * @param[in] ptr Pointer to the data.
 * @param[in] size Amount of data to move.
 * @return On success, returns 0. Otherwise returns an error code that
 * probably can't be figured out in practice, due to the large number of
 * possible implementations.
 */
int sicm_move(sicm_device* src, sicm_device* dst, void* ptr, size_t size);

/// Pins the current process to the processors closest to the memory.
/**
 * @param[in] device Device to pin the process to.
 */
int sicm_pin(sicm_device* device);

/// Query capacity of a device a device.
/**
 * @param[in] device Pointer to the sicm_device to query.
 * @return Capacity in kibibytes on the device.
 */
size_t sicm_capacity(sicm_device* device);

/// Query amount of available memory on a device.
/**
 * @param[in] device Pointer to the sicm_device to query.
 * @return Number of available kibibytes on the device.
 *
 * Note that this does not account for memory that has been allocated
 * but not yet touched.
 */
size_t sicm_avail(sicm_device* device);

/// Returns a distance metric based on general beliefs about the device/its location in the system.
/**
 * @param[in] device Pointer to the sicm_device to query.
 * @return Model distance metric. For example, on ordinary NUMA devices, this returns the distance
 * (as reported by the Linux NUMA subsystem) between the device and the CPU executing the calling
 * thread. For other devices, this should be calculated in the spirit of the NUMA subsystem distance
 * metric.
 *
 * There are pros and cons of model versus empirical distances: Model
 * metrics are "pure," in the sense that they are independent of
 * transient phenomena. Empirical metrics, by necessity, capture the
 * reality of the situation in which the measurement is taken, but
 * generalizing them requires capturing "more signal than noise," i.e.,
 * having more periodic than transient background phenomena. Thus, the
 * low-level interface exposes both empirical and model metrics to allow
 * higher-level interface designers to reach their own conclusions.
 */
int sicm_model_distance(sicm_device* device);

/// Indicates whether a device is on a near NUMA node.
/**
 * @param[in] device Pointer to the sicm_device to query.
 * @return Boolean indicating whether the device is a near node.
 *
 * Always returns 0 if the device is not a NUMA device.
 */
int sicm_is_near(sicm_device* device);

/// Measure empirical latency of the device.
/**
 * @param[in] device Pointer to the sicm_device to query.
 * @param[in] size Amount of memory to allocate for the test.
 * @param[in] iter Number of iterations to use for the test.
 * @param[out] res Pointer to a sicm_timing to store results.
 *
 * First, an allocation of the indicated size is created. Then, data are
 * written to iter random positions in the allocation. Then, data are
 * read from iter random positions in the allocation. Finally, the
 * allocation is freed. The time to complete each process is recorded in
 * res.
 */
void sicm_latency(sicm_device* device, size_t size, int iter, struct sicm_timing* res);

/// Measure empirical bandwidth, using linear access on a kernel function of arity 2.
/**
 * @param[in] device Pointer to the sicm_device to query.
 * @param[in] size Amount of memory to allocate.
 * @param[in] kernel Function that accesses the allocations in some way.
 * Returns the number of bytes accessed during the kernel.
 * @return Observed bandwidth over the course of the call to the kernel.
 *
 * This function allocates two arrays of doubles and passes them to the
 * kernel, along with the size of the allocations. The kernel must
 * return the number of bytes accessed by the kernel; typically, this
 * will be (2 * sizeof(double) * size), but it could be different for
 * different kernels. The time to complete the kernel is recorded, and
 * the return value is (bytes accessed by kernel) / (time to complete
 * kernel).
 */
size_t sicm_bandwidth_linear2(sicm_device* device, size_t size,
  size_t (*kernel)(double* a, double* b, size_t size));

/// Measure empirical bandwidth, using random access on a kernel function of arity 2.
/**
 * @param[in] device Pointer to the sicm_device to query.
 * @param[in] size Amount of memory to allocate.
 * @param[in] kernel Function that accesses the allocations in some way.
 * The third parameter (indexes) is an array of indexes into a and b.
 * Returns the number of bytes accessed during the kernel.
 * @return Observed bandwidth over the course of the call to the kernel.
 *
 * This function allocates two arrays of doubles and an array of random
 * indexes and passes them to the kernel, along with the size of the
 * allocations. The kernel must return the number of bytes accessed by
 * the kernel; typically, this will be
 * ((2 * sizeof(double) + sizeof(size_t)) * size), but it could be
 * different for different kernels. The time to complete the kernel is
 * recorded, and the return value is (bytes accessed by kernel) / (time
 * to complete kernel).
 */
size_t sicm_bandwidth_random2(sicm_device* device, size_t size,
  size_t (*kernel)(double* a, double* b, size_t* indexes, size_t size));

/// Measure empirical bandwidth, using linear access on a kernel function of arity 3.
/**
 * @param[in] device Pointer to the sicm_device to query.
 * @param[in] size Amount of memory to allocate.
 * @param[in] kernel Function that accesses the allocations in some way.
 * Returns the number of bytes accessed during the kernel.
 * @return Observed bandwidth over the course of the call to the kernel.
 *
 * This function allocates three arrays of doubles and passes them to
 * the kernel, along with the size of the allocations. The kernel must
 * return the number of bytes accessed by the kernel; typically, this
 * will be (3 * sizeof(double) * size), but it could be different for
 * different kernels. The time to complete the kernel is recorded, and
 * the return value is (bytes accessed by kernel) / (time to complete
 * kernel).
 */
size_t sicm_bandwidth_linear3(sicm_device* device, size_t size,
  size_t (*kernel)(double* a, double* b, double* c, size_t size));

/// Measure empirical bandwidth, using random access on a kernel function of arity 3.
/**
 * @param[in] device Pointer to the sicm_device to query.
 * @param[in] size Amount of memory to allocate.
 * @param[in] kernel Function that accesses the allocations in some way.
 * The third parameter (indexes) is an array of indexes into a and b.
 * Returns the number of bytes accessed during the kernel.
 * @return Observed bandwidth over the course of the call to the kernel.
 *
 * This function allocates three arrays of doubles and an array of
 * random indexes and passes them to the kernel, along with the size of
 * the allocations. The kernel must return the number of bytes accessed
 * by the kernel; typically, this will be
 * ((3 * sizeof(double) + sizeof(size_t)) * size), but it could be
 * different for different kernels. The time to complete the kernel is
 * recorded, and the return value is (bytes accessed by kernel) / (time
 * to complete kernel).
 */
size_t sicm_bandwidth_random3(sicm_device* device, size_t size,
  size_t (*kernel)(double* a, double* b, double* c, size_t* indexes, size_t size));

/// Linear-access triad kernel for use with sicm_bandwidth_linear3.
/**
 * This is modeled after the STREAM benchmark: for all indexes, computes
 * { a[i] = b[i] + scalar * c[i]; } and returns the number of accesses,
 * i.e., (3 * sizeof(double) * size).
 *
 * This can be used as a template for other bandwidth kernels.
 */
size_t sicm_triad_kernel_linear(double* a, double* b, double* c, size_t size);

/// Random-access triad kernel for use with sicm_bandwidth_random3.
/**
 * This is modeled after the STREAM benchmark: for all indexes, computes
 * { size_t idx = indexes[i]; a[idx] = b[idx] + scalar * c[idx]; } and
 * returns the number of accesses, i.e.,
 * ((3 * sizeof(double) + sizeof(size_t)) * size).
 *
 * This can be used as a template for other bandwidth kernels.
 */
size_t sicm_triad_kernel_random(double* a, double* b, double* c, size_t* indexes, size_t size);

#endif
