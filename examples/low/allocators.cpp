#include <cerrno>
#include <cstring>
#include <deque>
#include <iostream>
#include <list>
#include <map>
#include <vector>

#include "sicm.hpp"
#include "check_node.hpp"

const std::size_t size_exponent   = 5;
const std::size_t size_multiplier = 10;

template <template <typename> typename Allocator, typename T>
bool allocator_test(sicm_device_list *devs) {
    bool correct = true;

    for(unsigned i = 0; i < devs->count; i++) {
        sicm_device *dev = devs->devices[i];

        if (dev->page_size != 4) {
            continue;
        }

        bool curr = true;

        // deque
        {
            Allocator <T> sa(dev);

            std::size_t size = 1;
            for(std::size_t j = 0; j < size_exponent; j++) {
                std::deque <T, Allocator <T> > deque(sa);
                deque.resize(size);
                size *= size_multiplier;

                if (!check_location(dev, deque)) {
                    std::cerr << "std::deque item was not in the correct location" << std::endl;
                    curr = false;
                }
            }
        }

        // list
        {
            Allocator <T> sa(dev);

            std::size_t size = 1;
            for(std::size_t j = 0; j < size_exponent; j++) {
                std::list <T, Allocator <T> > list(sa);
                list.resize(size);
                size *= size_multiplier;

                if (!check_location(dev, list)) {
                    std::cerr << "std::list item was not in the correct location" << std::endl;
                    curr = false;
                }
            }
        }

        // map
        {
            Allocator <std::pair <const T, T> > sa(dev);

            std::map <T, T, std::less <T>,
                      Allocator <std::pair<const T, T> > > map(sa);
            for(std::size_t j = 0; j < size_exponent; j++) {
                map[i] = i;
            }

            if (!check_location(dev, map)) {
                std::cerr << "std::map item was not in the correct location" << std::endl;
                curr = false;
            }
        }

        // vector
        {
            Allocator <T> sa(dev);

            std::size_t size = 1;
            for(std::size_t j = 0; j < size_exponent; j++) {
                std::vector <T, Allocator <T> > vector(sa);
                vector.resize(size);
                size *= size_multiplier;

                if (!check_location(dev, vector)) {
                    std::cerr << "std::vector item was not in the correct location" << std::endl;
                    curr = false;
                }
            }
        }

        if (curr) {
            std::cout << "All allocations were located on device " << i
                      << " (" << sicm_device_tag_str(dev->tag)
                      << ", NUMA node " << dev->node
                      << ", " << dev->page_size << " KB)" << std::endl;
        }
        else {
            std::cerr << "Not all allocations were located on device " << i
                      << " (" << sicm_device_tag_str(dev->tag)
                      << ", NUMA node " << dev->node
                      << ", " << dev->page_size << " KB)" << std::endl;
        }

        correct &= curr;
    }

    return correct;
}

int main() {
    sicm_device_list devs = sicm_init();

    const int rc = allocator_test<SICMArenaAllocator,  int>(&devs) &&
                   allocator_test<SICMDeviceAllocator, int>(&devs);

    sicm_fini();

    return !rc; // invert since 0 and not-0 are inverted for return values
}
