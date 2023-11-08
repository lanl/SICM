#include "detect_devices/sycl.h"
#include "CL/sycl.hpp"

static bool sycl_device_usable(const sycl::device &dev) {
    /* can be gpu or accelerator */
    if (dev.is_host() ||
        dev.is_cpu()) {
        return false;
    }

    /* no use if not available */
    if (!dev.get_info<sycl::info::device::is_available>()) {
        return false;
    }

    // /* on a numa node */
    // bool numa_found = false;
    // for(auto domain : dev.get_info<sycl::info::device::partition_affinity_domains>()) {
    //     numa_found |= (domain == sycl::info::partition_affinity_domain::numa);
    // }
    // if (!numa_found) {
    //     return false;
    // }

    return (dev.get_info<sycl::info::device::host_unified_memory>() &&
            dev.get_info<sycl::info::device::max_mem_alloc_size>());
}

int get_sycl_node_count() {
    int count = 0;

    auto platforms = sycl::platform::get_platforms();

    for (auto &platform : platforms) {
        auto devices = platform.get_devices();
        for(auto &device : devices) {
            count += sycl_device_usable(device);
        }
    }

    return count;
}

void detect_sycl(struct bitmask* compute_nodes, struct bitmask* non_dram_nodes,
                 int *huge_page_sizes, int huge_page_size_count, int normal_page_size,
                 struct sicm_device **devices, int *curr_idx) {
    int idx = *curr_idx;

    auto platforms = sycl::platform::get_platforms();

    for (auto &platform : platforms) {
        auto sycl_devices = platform.get_devices();
        for (auto &sycl_device : sycl_devices) {
            if (sycl_device_usable(sycl_device)) {
                sycl::context *ctx = new sycl::context(sycl_device);

                devices[idx]->tag = SICM_SYCL;
                devices[idx]->node = -1;
                devices[idx]->page_size = normal_page_size;
                devices[idx]->data.sycl = (struct sicm_sycl_data){ .device = &sycl_device, .context = ctx };
                idx++;
                for(int j = 0; j < huge_page_size_count; j++) {
                    devices[idx]->tag = SICM_SYCL;
                    devices[idx]->node = -1;
                    devices[idx]->page_size = huge_page_sizes[j];
                    devices[idx]->data.sycl = (struct sicm_sycl_data){ .device = &sycl_device, .context = ctx };
                    idx++;
                }
            }
        }
    }

    *curr_idx = idx;
}

void *sicm_alloc_sycl_device(struct sicm_device *device, size_t size) {
    return sycl::malloc_device(size,
                               * (sycl::device *) device->data.sycl.device,
                               * (sycl::context *) device->data.sycl.context);
}
