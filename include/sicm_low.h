/**
 * \file sicm_low.h
 * \brief Front end of the low-level interface.
 * 
 * Defines most functions that users (e.g., high-level interface or
 * low-level application/run-time designers) should interact with.
 * Chiefly, this means initialization of the low-level data structures,
 * wrappers around vtable functions, and some common functions that
 * themselves refer to the vtable functions.
 */

#pragma once
/// Needed for use of sched_getcpu.
#define _GNU_SOURCE
#include <assert.h>
#include <errno.h>
#include <fcntl.h>
#include <numa.h>
#include <sched.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <sys/mman.h>
#include <time.h>
#include <unistd.h>

/// Amount of memory to allocate for the latency test.
#define SICM_LATENCY_SIZE 4096
/// Number of iterations for the latency test.
#define SICM_LATENCY_ITERATIONS 10000

/// Linear-feedback shift register PRNG algorithm.
/**
 * @param[in] n The previous random number/initial seed. Note that this
 * should be unsigned.
 * 
 * This is obviously not a cryptographic algorithm, or even very good
 * for intense random number exercises, but we just want something
 * that'll produce cache misses and avoid prefetching. This is done for
 * speed (to avoid polluting timings) and purity (for thread safety).
 * This function must be seeded manually.
 */
#define sicm_rand(n) \
  n = (n >> 1) ^ (unsigned int)((0 - (n & 1u)) & 0xd0000001u)

/// Brief description of a memory device.
/**
 * This should generally correspond to device include files, as in, each
 * field in the enumeration should have corresponding .h/.c files, and,
 * at least, distinct detection criteria.
 */
enum sicm_device_type {
  SICM_KNL_HBM, ///< Intel Knights Landing (Xeon Phi x200).
  SICM_DRAM ///< Ordinary RAM.
};

/// Indication of how to move data to or from this device.
/**
 * Actually moving data between devices requires handling all pairs in
 * this enumeration. The movement function, sicm_move, is responsible
 * for covering all such pairs and providing an implementation. Also,
 * this enumeration is used as a tag to correctly downcast the
 * sicm_mover_payload union.
 */
enum sicm_mover_type {
  SICM_MOVER_NUMA ///< NUMA device, uses libNUMA.
};

/// Additional information required to process data movement.
/**
 * Allows heterogeneous storage of different data for different memory
 * devices. The inhabited type of the union corresponds to an entry in
 * the sicm_mover_type enumeration; specifically, the inhabited type
 * should have the same name, minus the SICM_MOVER_ prefix, and in lower
 * case.
 */
union sicm_mover_payload {
  int numa; ///< NUMA node of a memory device.
};

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

/// Representation of a memory device.
/**
 * This structure contains information to allow users to identify what a
 * memory device is, as well as pointers to functions that implement the
 * fundamental per-device operations. In other words, this structure is
 * the way that one interacts with a memory device. See the associated
 * wrapper functions (e.g., sicm_alloc, sicm_free) for more detail.
 */
struct sicm_device {
  enum sicm_device_type ty; ///< Type of device (see sicm_device_type).
  enum sicm_mover_type move_ty;
    ///< How data is moved to and from this device (see sicm_mover_type).
  union sicm_mover_payload move_payload;
    ///< Additional information required for movement (see sicm_mover_type and sicm_mover_payload).
  void* (*alloc)(struct sicm_device*, size_t); ///< Function to allocate on this device.
  void (*free)(struct sicm_device*, void*, size_t); ///< Function to deallocate/free on this device.
  size_t (*used)(struct sicm_device*); ///< Queries the amount of used (i.e., physically-backed) memory on this device.
  size_t (*capacity)(struct sicm_device*); ///< Queries the capacity of the device.
  int (*model_distance)(struct sicm_device*);
    ///< Returns a distance metric based on general beliefs about the device/its location in the system.
};

/// Functions that help find memory devices of a particular type.
/**
 * Each memory device type (e.g., DRAM, KNL HBM) must implement this
 * structure, to allow unified detection of devices of that type.
 * These functions are ultimately used to populate the singleton
 * sicm_device_list.
 */
struct sicm_device_spec {
  int (*non_numa_count)(); ///< Return the number of devices of this type that aren't NUMA nodes.
  int (*add_devices)(struct sicm_device* device_list, int idx, struct bitmask* numa_mask);
    ///< Add all devices of this type to an array.
    /**<
     * @param[in,out] device_list Partly-constructed list of memory devices.
     * @param[in] idx Current index in device_list.
     * @param[in,out] numa_mask Bitmask of NUMA nodes taken by specialized devices
     * @return New index in device_list.
     * 
     * Given a sicm_device array and a starting offset: populate the array with all known devices
     * of this type, set bits in numa_mask for each NUMA node occupied by a device of this type,
     * and return the new offset. A corresponding function is called for all known device types,
     * and any unset bits in numa_mask are treated as ordinary RAM.
     * 
     * device_list is allocated to be able to hold (number of NUMA nodes) + (sum of all calls to
     * non_numa_count).
     */
};

/// Explicitly-sized sicm_device array.
struct sicm_device_list {
  unsigned int count; ///< Number of devices in the array.
  struct sicm_device* devices; ///< Array of devices of count elements.
};

/// Function that returns 0.
/**
 * This helper function can be used to fill the non_numa_count field of
 * a sicm_device_spec that only uses NUMA domains (i.e., so that
 * non_numa_count should always return 0).
 */
int zero();

/// Returns a bitmask of NUMA nodes that have CPUs on them.
/**
 * Some memory devices are exposed as NUMA nodes that don't contain
 * CPUs, so this function allows that information to be memoized.
 */
struct bitmask* sicm_cpu_mask();

/// Initialize the low-level interface.
/**
 * Determine the total number of memory devices (which is the number of
 * NUMA nodes plus the sum of all calls to non_numa_count) and
 * allocates a sicm_device_list. Then add_devices is called on all
 * sicm_device_spec using the sicm_device array.
 * 
 * More cleverness notwithstanding, the first two lines register all
 * sicm_device_spec and must be modified as more specs are added.
 */
struct sicm_device_list sicm_init();

/// Allocate memory on a SICM device.
/**
 * @param[in] device Pointer to a sicm_device to allocate on.
 * @param[in] size Amount of memory to allocate.
 * @return Pointer to the start of the allocation.
 */
void* sicm_alloc(struct sicm_device* device, size_t size);

/// Deallocate/free memory on a SICM device.
/**
 * @param[in] device Pointer to the sicm_device the allocation was made on.
 * @param[in] ptr Pointer to the start of the allocation (i.e., return value of sicm_alloc).
 * @param[in] size Amount of memory to deallocate (should be the same as the allocation).
 */
void sicm_free(struct sicm_device* device, void* ptr, size_t size);

/// Query amount of used (physically-backed) memory on a device.
/**
 * @param[in] device Pointer to the sicm_device to query.
 * @return Number of already-used bytes on the device.
 */
size_t sicm_used(struct sicm_device* device);

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
int sicm_model_distance(struct sicm_device* device);

/// Measure empirical latency of the device.
/**
 * @param[in] device Pointer to the sicm_device to query.
 * @param[out] res Pointer to a sicm_timing to store results.
 * 
 * First, an allocation of size SICM_LATENCY_SIZE is created. Then,
 * data are written to SICM_LATENCY_ITERATIONS random positions in the
 * allocation. Then, data are read from SICM_LATENCY_ITERATIONS random
 * positions in the allocation. Finally, the allocation is freed. The
 * time to complete each process is recorded in res.
 */
void sicm_latency(struct sicm_device* device, struct sicm_timing* res);

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
size_t sicm_bandwidth_linear2(struct sicm_device* device, size_t size, size_t (*kernel)(double* a, double* b, size_t size));

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
size_t sicm_bandwidth_random2(struct sicm_device* device, size_t size, size_t (*kernel)(double* a, double* b, size_t* indexes, size_t size));

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
size_t sicm_bandwidth_linear3(struct sicm_device* device, size_t size, size_t (*kernel)(double* a, double* b, double* c, size_t size));

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
size_t sicm_bandwidth_random3(struct sicm_device* device, size_t size, size_t (*kernel)(double* a, double* b, double* c, size_t* indexes, size_t size));

/// Move data from one device to another.
/**
 * @param[in] src Device that currently contains the data.
 * @param[in] dst Device that ought to contain the data.
 * @param[in] ptr Pointer to the data.
 * @param[in] size Amount of data to move.
 * @return On success, returns 0. Otherwise returns an error code that
 * probably can't be figured out in reality, due to the large number of
 * possible implementations.
 */
int sicm_move(struct sicm_device* src, struct sicm_device* dst, void* ptr, size_t size);

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
