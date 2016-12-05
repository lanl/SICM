/**
 * \file dram.h
 * \brief Defines the sicm_device_spec and sicm_device for ordinary DRAM.
 */

#pragma once

#include "sicm_low.h"

/// Returns the sicm_device_spec for DRAM.
/**
 * The value for non_numa_count is zero. For add_devices, the
 * implementation file dram.c defines a private function sicm_dram_add.
 * This function simply claims all unclaimed NUMA nodes as DRAM nodes,
 * and assigns the functions from numa_common.h
 */
struct sicm_device_spec sicm_dram_spec();
