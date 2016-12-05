/**
 * \file numa_common.h
 * \brief Implementations for sicm_device function calls that defer to libNUMA.
 */

#pragma once

#include "sicm_low.h"

/// Call for sicm_alloc that invokes numa_alloc_onnode
void* sicm_numa_common_alloc(struct sicm_device*, size_t);
/// Call for sicm_free that invokes numa_free
void sicm_numa_common_free(struct sicm_device*, void*, size_t);
/// Call for sicm_used that queries sysfs
/**
 * The NUMA node ID is taken from the sicm_mover_payload numa field. The
 * actual data are taken from /sys/devices/system/node/nodeID/meminfo
 */
size_t sicm_numa_common_used(struct sicm_device*);
/// Call for sicm_capacity that queries sysfs
/**
 * The NUMA node ID is taken from the sicm_mover_payload numa field. The
 * actual data are taken from /sys/devices/system/node/nodeID/meminfo
 */
size_t sicm_numa_common_capacity(struct sicm_device*);
/// Call for sicm_model_distance that invokes numa_distance
/**
 * The memory NUMA node ID is taken from sicm_mover_payload numa field,
 * while the CPU NUMA node ID is numa_node_of_cpu(sched_getcpu())
 */
int sicm_numa_common_model_distance(struct sicm_device*);
