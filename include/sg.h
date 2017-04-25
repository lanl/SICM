/**
 * \file sg.h
 * \brief SICM "greedy" high-level interface.
 *
 * The greedy high-level interface assumes that allocations are presented in
 * order of importance, so that the best thing to do is to always do the best
 * current allocation. An alternative allocation strategy is provided, where
 * larger stores are preferred over performant ones, in case it becomes useful
 * to do a less important allocation up front.
 *
 * This library provides two kinds of allocations: exact and spill. An exact
 * allocation will take place entirely on one memory device, so in the case of a
 * performance-focused allocation, it will take the fastest device that can
 * entirely handle the allocation. A spill allocation will potentially split the
 * allocation among multiple devices, though it will ensure a contiguous address
 * space. The drawback to spill allocations is that it will always allocate in
 * page-size chunks, so it's unsuitable for small allocations.
 *
 * The main functions in this library are surrounded in OMP CRITICAL blocks,
 * because they need to have a reasonably accurate picture of the available
 * memory. Concurrent allocations could potentially conflict with the others'
 * modeling. Also, they need shared access to an allocation table, though that
 * could be done with finer-grained locking if not for the other concerns.
 */
#pragma once

//#include <stddef.h>
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
 * @param[in] id A number identifying the calling process, for example, MPI
 * rank. The numbers should probably be "counting" numbers, i.e., uniformly
 * distributed with each number within 1 of one or two other numbers.
 *
 * This function calls sicm_init, and also populates some internal data
 * structures.
 */
void sg_init(int id);

/// Allocate an exact amount of memory on the fastest available device.
/**
 * @param[in] sz The amount of memory to allocate.
 * @return The start of the allocation, or NULL on failure.
 *
 * This function ensures that all the memory will reside on the same device.
 * Mainly, this is much faster than sg_alloc_perf.Note that this will "touch"
 * each page of the allocation.
 *
 * This function is OMP CRITICAL with the other sg_alloc functions.
 */
void* sg_alloc_exact(size_t sz);

/// Allocate memory that spills between devices, biased toward performance.
/**
 * @param[in] sz The amount of memory to allocate.
 * @return The start of the memory, or NULL on failure.
 *
 * This function will allocate some amount of memory, using as much as is
 * available on each device, sorted by performance. Note that this will "touch"
 * each page of the allocation.
 *
 * This function is OMP CRITICAL with the other sg_alloc functions.
 */
void* sg_alloc_perf(size_t sz);

/// Allocate memory that spills between devices, biased toward capacity.
/**
 * @param[in] sz The amount of memory to allocate.
 * @return The start of the memory, or NULL on failure.
 *
 * This function will allocate some amount of memory, using as much as is
 * available on each device, but preferring low-performance memory. Note that
 * this will "touch" each page of the allocation.
 *
 * This function is OMP CRITICAL with the other sg_alloc functions.
 */
void* sg_alloc_cap(size_t sz);

/// Free memory that was allocated with this library.
/**
 * @param[in] ptr The start of the allocation.
 *
 * This function is OMP CRITICAL with the sg_alloc functions.
 */
void sg_free(void* ptr);
