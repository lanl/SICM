#pragma once

#include "sicm_low.h"

/// Memory device priority list, sorted by performance.
/**
 * This library uses a "common wisdom" ordering of memory devices, and this list
 * (which is filled by sg_init) contains our ordering, given the actual devices
 * found. This is exposed in the header file in case anyone's interested in it.
 */
extern struct sicm_device_list sg_performance_list;

/// Memory device priority list, sorted by capacity.
/**
 * This is nearly the inverse of sg_performance_list, but it'll still prefer to
 * allocate on the closest NUMA node.
 */
extern struct sicm_device_list sg_capacity_list;

/// Initialize the high-level interface.
/**
 * This function calls sicm_init, and also populates some internal data
 * structures.
 */
__attribute__((constructor))
void sg_init();

__attribute__((destructor))
void sg_terminate();

/// Allocate an exact amount of memory on the fastest available device.
/**
 * @param[in] id The allocation site ID.
 * @param[in] sz The amount of memory to allocate.
 * @return The start of the allocation, or NULL on failure.
 *
 * This function ensures that all the memory will reside on the same device.
 * Mainly, this is much faster than sg_alloc_perf.Note that this will "touch"
 * each page of the allocation.
 *
 * This function is OMP CRITICAL with the other sg_alloc functions.
 */
void* sg_alloc_exact(int id, size_t sz);

/// Free memory that was allocated with this library.
/**
 * @param[in] ptr The start of the allocation.
 *
 * This function is OMP CRITICAL with the sg_alloc functions.
 */
void sg_free(void* ptr);
