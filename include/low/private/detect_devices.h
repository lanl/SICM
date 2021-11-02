#ifndef __SICM_DETECT_DEVICES_H
#define __SICM_DETECT_DEVICES_H

#include "detect_devices/x86.h"
#include "detect_devices/powerpc.h"
#include "detect_devices/DRAM.h"

int detect_devices(int node_count,
                   int *huge_page_sizes, int huge_page_size_count, int normal_page_size,
                   struct sicm_device **devices);

#endif
