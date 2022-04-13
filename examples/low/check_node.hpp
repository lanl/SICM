#ifndef CHECK_NODE_HPP
#define CHECK_NODE_HPP

#ifdef HIP
#include <hip/hip_runtime.h>
#endif
#include <iostream>
#include <numaif.h>

#include "sicm_low.h"

// check location of elements within the container, not the container itself
template <template <typename ...> typename Container, typename ... T>
bool check_location(sicm_device *dev, const Container <T...> &data) {
    for(typename Container<T...>::value_type const &item : data) {
        void *ptr =  (void *) &item;

        #ifdef HIP
        // move_pages doesn't work on hip pointers
        if (dev->tag == SICM_HIP) {
            hipPointerAttribute_t attrs;
            int err = hipPointerGetAttributes(&attrs, ptr);
            if (err != hipSuccess) {
                std::cerr << "hipPointerGetAttributes failed:" << err << std::endl;
                continue;
            }

            if (dev->data.hip.id != attrs.device) {
                return false;
            }
        }
        else {
        #endif
            int loc = -1;
            if (move_pages(0, 1, &ptr, nullptr, &loc, 0) != 0) {
                const int err = errno;
                std::cerr << "move_pages failed: " << strerror(err) << ": ("  << err << ")" << std::endl;
                continue;
            }

            if (dev->node != loc) {
                return false;
            }
        #ifdef HIP
        }
        #endif
    }

    return true;
}

#endif
