#include <cerrno>
#include <cstring>
#include <deque>
#include <iostream>
#include <list>
#include <map>
#include <numaif.h>
#include <vector>

#include <sicm.hpp>

const std::size_t size_exponent   = 5;
const std::size_t size_multiplier = 10;

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

int main() {
    sicm_device_list devs = sicm_init();

    for(unsigned i = 0; i < devs.count; i += 3) {
        sicm_device *dev = devs.devices[i];
        SICMArenaAllocator<int> sa(dev); // use this SICMArenaAllocator instance for all containers

        bool correct = true;

        // deque
        {
            std::size_t size = 1;
            for(std::size_t j = 0; j < size_exponent; j++) {
                std::deque <int, SICMArenaAllocator <int> > deque(sa);
                deque.resize(size);
                size *= size_multiplier;

                if (!check_location(dev, deque)) {
                    std::cerr << "std::deque item was not in the correct location" << std::endl;
                    correct = false;
                }
            }
        }

        // list
        {
            std::size_t size = 1;
            for(std::size_t j = 0; j < size_exponent; j++) {
                std::list <int, SICMArenaAllocator <int> > list(sa);
                list.resize(size);
                size *= size_multiplier;

                if (!check_location(dev, list)) {
                    std::cerr << "std::list item was not in the correct location" << std::endl;
                    correct = false;
                }
            }
        }

        // map
        {
            std::map <int, int, std::less <int>, SICMArenaAllocator <std::pair<const int, int> > > map(sa);
            for(std::size_t j = 0; j < size_exponent; j++) {
                map[i] = i;
            }

            if (!check_location(dev, map)) {
                std::cerr << "std::map item was not in the correct location" << std::endl;
                correct = false;
            }
        }

        // vector
        {
            std::size_t size = 1;
            for(std::size_t j = 0; j < size_exponent; j++) {
                std::vector <int, SICMArenaAllocator <int> > vector(sa);
                vector.resize(size);
                size *= size_multiplier;

                if (!check_location(dev, vector)) {
                    std::cerr << "std::vector item was not in the correct location" << std::endl;
                    correct = false;
                }
            }
        }

        if (correct) {
            std::cout << "All allocations were located on NUMA node " << dev->node << std::endl;
        }
        else {
            std::cerr << "Not all allocations were located on NUMA node " << dev->node << std::endl;
        }
    }

    sicm_fini();
    return 0;
}
