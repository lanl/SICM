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
#pragma once

/// Prerequisite for sicm_model_distance.
#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif
#include <stdint.h>
#include <stdlib.h>

/// Linear-feedback shift register PRNG algorithm.
/**
 * @param[in,out] n The previous random number/initial seed. This should
 * be unsigned.
 *
 * This is obviously not a cryptographic algorithm, or even very good
 * for intense random number exercises, but we just want something
 * that'll produce cache misses and avoid prefetching. This is done for
 * speed (to avoid polluting timings) and purity (for thread safety).
 * This function must be seeded manually. time(NULL) can be used, or if
 * multiple threads will be launched nearby in time, you can the address
 * of a local variable.
 *
 * The "magic number" 0xd0000001u is the maximum-period internal XOR
 * value for the 64-bit Galois LSFR.
 */
#define sicm_rand(n) \
  n = (n >> 1) ^ (unsigned int)((0 - (n & 1u)) & 0xd0000001u)

/// FNV-1A hash algorithm.
/**
 * @param[in] n The value to be hashed.
 * @return The hash of n.
 *
 * This is a simple but reasonably effective hash algorithm. It's mainly
 * useful for generating random index arrays, particularly if the index
 * array is being generated in parallel. So this is preferred if you
 * have a large amount of input values to transform, while if you have a
 * single value that you want to process on repeatedly, sicm_rand is
 * preferred. Of course, this is not a cryptographic algorithm.
 *
 * The "magic numbers" were chosen by the original authors (Fowler,
 * Noll, and Vo) for good hashing behavior. These numbers are
 * specifically tailored to 64-bit integers.
 */
#define sicm_hash(n) ((0xcbf29ce484222325 ^ (n)) * 0x100000001b3)

/// Ceiling of integer division.
/**
 * @param[in] n Numerator (dividend).
 * @param[in] d Denominator (divisor).
 * @return Quotient, rounded toward positive infinity.
 *
 * Ordinary integer division implicitly floors, so 1 / 2 = 0. This
 * instead returns the ceiling, so 1 / 2 = 1. This works by just
 * adding 1 if the division produces a nonzero remainder.
 */
#define sicm_div_ceil(n, d) ((n) / (d) + ((n) % (d) ? 1 : 0))

/// System page size in KiB.
/**
 * This variable is initialized by sicm_init(), so don't use it before
 * then.
 */
extern int normal_page_size;

/// Enumeration of supported memory device types.
/**
 * This tag can be used to identify a device, but its internal role is
 * to indicate which case of the sicm_device_data union is inhabited.
 */
enum sicm_device_tag {
  SICM_DRAM,
  SICM_KNL_HBM
};

/// Data specific to a DRAM device.
struct sicm_dram_data {
  int node; ///< NUMA node
  int page_size; ///< Page size
};

/// Data specific to a KNL HBM device.
struct sicm_knl_hbm_data {
  int node; ///< NUMA node
  int compute_node;
  int page_size; ///< Page size
};

/// Data that, given a device type, uniquely identify the device within that type.
/**
 * This union is only meaningful in the presence of a sicm_device_tag,
 * and together they identify a particular memory device. The
 * sicm_device_tag enumeration indicates which case of this union is
 * inhabited.
 */
union sicm_device_data {
  struct sicm_dram_data dram;
  struct sicm_knl_hbm_data knl_hbm;
};

/// Tagged/discriminated union that fully identifies a device.
/**
 * The combination of a sicm_device_tag and sicm_device_data identifies
 * a device. Given this, heterogeneous functions on memory devices
 * should switch on the tag, then use the data to further refine their
 * operations.
 */
struct sicm_device {
  enum sicm_device_tag tag; ///< Type of memory device
  union sicm_device_data data; ///< Per-type identifying information
};

/// Explicitly-sized sicm_device array.
struct sicm_device_list {
  unsigned int count; ///< Number of devices in the array.
  struct sicm_device* devices; ///< Array of devices of count elements.
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

/// Initialize the low-level interface.
/**
 * Determine the total number of memory devices (which is the number of
 * NUMA nodes times the number of huge page sizes, plus the sum of all
 * non-NUMA memory devices such as CUDA GPUs) and allocates a
 * sicm_device_list. Per-device detection criteria are used to populate
 * the device list, which is then returned.
 *
 * There's no explicit way to free the device list, though
 * free(device_list.devices) would work, because it's assumed this
 * function is called once and the device list is needed for the entire
 * program lifetime.
 */
struct sicm_device_list sicm_init();

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
void* sicm_alloc(struct sicm_device* device, size_t size);

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
void* sicm_alloc_exact(struct sicm_device* device, void* base, size_t size);

/// Deallocate/free memory on a SICM device.
/**
 * @param[in] device Pointer to the sicm_device the allocation was made on.
 * @param[in] ptr Pointer to the start of the allocation (i.e., return value of sicm_alloc).
 * @param[in] size Amount of memory to deallocate (should be the same as the allocation).
 */
void sicm_free(struct sicm_device* device, void* ptr, size_t size);

/// Get the NUMA node number of a SICM device.
/**
 * @param[in] device Pointer ot the sicm_device to query.
 * @return NUMA node number of the device. If the device is not a NUMA node, the return value is -1.
 */
int sicm_numa_id(struct sicm_device* device);

/// Get the page size of a SICM device.
/**
 * @param[in] device Pointer ot the sicm_device to query.
 * @return Page size of the device. If the device is not a NUMA node, the return value is -1.
 */
int sicm_device_page_size(struct sicm_device* device);

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
int sicm_move(struct sicm_device* src, struct sicm_device* dst, void* ptr, size_t size);

/// Pins the current process to the processors closest to the memory.
/**
 * @param[in] device Device to pin the process to.
 */
void sicm_pin(struct sicm_device* device);

/// Query capacity of a device a device.
/**
 * @param[in] device Pointer to the sicm_device to query.
 * @return Capacity in kibibytes on the device.
 */
size_t sicm_capacity(struct sicm_device* device);

/// Query amount of available memory on a device.
/**
 * @param[in] device Pointer to the sicm_device to query.
 * @return Number of available kibibytes on the device.
 *
 * Note that this does not account for memory that has been allocated
 * but not yet touched.
 */
size_t sicm_avail(struct sicm_device* device);

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

/// Indicates whether a device is on a near NUMA node.
/**
 * @param[in] device Pointer to the sicm_device to query.
 * @return Boolean indicating whether the device is a near node.
 *
 * Always returns 0 if the device is not a NUMA device.
 */
int sicm_is_near(struct sicm_device* device);

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
void sicm_latency(struct sicm_device* device, size_t size, int iter, struct sicm_timing* res);

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
size_t sicm_bandwidth_linear2(struct sicm_device* device, size_t size,
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
size_t sicm_bandwidth_random2(struct sicm_device* device, size_t size,
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
size_t sicm_bandwidth_linear3(struct sicm_device* device, size_t size,
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
size_t sicm_bandwidth_random3(struct sicm_device* device, size_t size,
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
