/**
 * \file knl_hbm.h
 * \brief Defines the sicm_device_spec and sicm_device for Knights Landing high-bandwidth memory.
 */

#pragma once

#include "sicm_low.h"

/// Returns the sicm_device_spec for KNL HBM.
/**
 * The value for non_numa_count is zero. For add_devices, the
 * implementation file knl_hbm.c defines a private function sicm_knl_hbm_add.
 * This function uses a call to cpuid to determine if the computer is a
 * Knights Landing (i.e., Xeon Phi x200). If it is, then any NUMA nodes
 * that do not have CPUs on them are treated as HBM. The functions for
 * the sicm_device are taken from numa_common.h
 */
struct sicm_device_spec sicm_knl_hbm_spec();
