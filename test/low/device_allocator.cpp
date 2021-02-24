#include <deque>
#include <list>
#include <map>
#include <vector>

#include <sicm.hpp>

int main() {
    sicm_device_list devs = sicm_init();

    // the actual check is not crashing

    // deque
    {
        std::size_t size = 1;
        for(std::size_t i = 0; i < 5; i++) {
            std::deque <int, SICMDeviceAllocator <int> > deque(size);
            size *= 10;
            deque.resize(size);
        }
    }

    // list
    {
        std::size_t size = 1;
        for(std::size_t i = 0; i < 5; i++) {
            std::list <int, SICMDeviceAllocator <int> > list(size);
            size *= 10;
            list.resize(size);
        }
    }

    // map
    {
        std::size_t size = 1;
        std::map <int, int, std::less <int>, SICMDeviceAllocator <std::pair<const int, int> > > map;
        for(std::size_t i = 0; i < 5; i++) {
            map[i] = i;
        }
    }

    // vector
    {
        std::size_t size = 1;
        for(std::size_t i = 0; i < 5; i++) {
            std::vector <int, SICMDeviceAllocator <int> > vector(size);
            size *= 10;
            vector.resize(size);
        }
    }

    sicm_fini();
    return 0;
}
