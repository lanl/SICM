#ifndef CHECK_NODE_HPP
#define CHECK_NODE_HPP

#include <iostream>
#include <numaif.h>

#include "sicm_low.h"

// check location of elements within the container, not the container itself
template <template <typename ...> typename Container, typename ... T>
bool check_location(sicm_device *dev, const Container <T...> &data) {
    for(typename Container<T...>::value_type const &item : data) {
        void *ptr =  (void *) &item;
        int loc = -1;
        if (move_pages(0, 1, &ptr, nullptr, &loc, 0) != 0) {
            const int err = errno;
            std::cerr << "move_pages failed: " << strerror(err) << ": ("  << err << ")" << std::endl;
            continue;
        }

        if (dev->node != loc) {
            return false;
        }
    }

    return true;
}

#endif
