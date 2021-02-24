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
            std::deque <int, SICMArenaAllocator <int> > deque(size);
            size *= 10;
            deque.resize(size);
        }
    }

    // list
    {
        std::size_t size = 1;
        for(std::size_t i = 0; i < 5; i++) {
            std::list <int, SICMArenaAllocator <int> > list(size);
            size *= 10;
            list.resize(size);
        }
    }

    // map
    {
        std::size_t size = 1;
        std::map <int, int, std::less <int>, SICMArenaAllocator <std::pair<const int, int> > > map;
        for(std::size_t i = 0; i < 5; i++) {
            map[i] = i;
        }
    }

    // vector
    {
        std::size_t size = 1;
        for(std::size_t i = 0; i < 5; i++) {
            std::vector <int, SICMArenaAllocator <int> > vector(size);
            size *= 10;
            vector.resize(size);
        }
    }

    sicm_fini();
    return 0;
}
