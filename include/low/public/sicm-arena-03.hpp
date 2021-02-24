#ifndef SICM_ARENA_CXX_03_ALLOCATOR
#define SICM_ARENA_CXX_03_ALLOCATOR

#include <limits>
#include <memory>
#include <pthread.h>

#include "sicm_low.h"

template <class T> class SICMArenaAllocator;

template <>
class SICMArenaAllocator<void>
{
public:
    typedef void              value_type;
    typedef value_type*       pointer;
    typedef value_type const* const_pointer;
    typedef std::size_t       size_type;
    typedef std::ptrdiff_t    difference_type;

    template <class U>
    struct rebind
    {
        typedef SICMArenaAllocator<U> other;
    };
};

template <class T>
class SICMArenaAllocator
{
public:
    typedef T                 value_type;
    typedef value_type&       reference;
    typedef value_type const& const_reference;
    typedef value_type*       pointer;
    typedef value_type const* const_pointer;
    typedef std::size_t       size_type;
    typedef std::ptrdiff_t    difference_type;

    template <class U>
    struct rebind
    {
        typedef SICMArenaAllocator<U> other;
    };

    template <class U>
    friend class SICMArenaAllocator;

    SICMArenaAllocator(sicm_arena a = ARENA_DEFAULT) throw() {
        arena = a;
        cleanup = false;
    }

    SICMArenaAllocator(sicm_device *dev,
                       const size_t max_size = 0) throw() {
        sicm_device_list devs = { .count = 1, .devices = &dev };
        arena = sicm_arena_create(max_size, 0, &devs);
        cleanup = true;
    }

    SICMArenaAllocator(sicm_device **dev_array, const size_t count,
                       const size_t max_size = 0) throw() {
        sicm_device_list devs = { .count = count, .devices = dev_array };
        arena = sicm_arena_create(max_size, 0, &devs);
        cleanup = true;
    }

    SICMArenaAllocator(sicm_device_list *dev_list,
                       const size_t max_size = 0) throw() {
        arena = sicm_arena_create(max_size, 0, dev_list);
        cleanup = true;
    }

    template <class U> SICMArenaAllocator(SICMArenaAllocator<U> const& u) throw() {
        arena = u.arena;
        cleanup = false;
    }

    ~SICMArenaAllocator() {
        if (cleanup) {
            sicm_arena_destroy(arena);
        }
    }

    pointer
    allocate(size_type n, SICMArenaAllocator<void>::const_pointer = 0)
    {
        void *mem = NULL;
        if (!(mem = sicm_arena_alloc(arena, n * sizeof(value_type)))) {
            throw std::bad_alloc();
        }

        return (pointer) mem;
    }

    void
    deallocate(pointer p, size_type)
    {
        sicm_free(p);
    }

    void
    construct(pointer p, value_type const& val)
    {
        ::new(p) value_type(val);
    }

    void
    destroy(pointer p)
    {
        p->~value_type();
    }

    size_type
    max_size() const throw()
    {
        return std::numeric_limits<size_type>::max() / sizeof(value_type);
    }

    pointer
    address(reference x) const
    {
        return &x;
    }

    const_pointer
    address(const_reference x) const
    {
        return &x;
    }

    int
    ChangeDevice(sicm_device *dev)
    {
        return sicm_arena_set_device(arena, dev);
    }

    int
    ChangeDevices(sicm_device **dev_array, const size_t count)
    {
        return sicm_arena_set_device_array(arena, dev_array, count);
    }

    int
    ChangeDevices(sicm_device_list *devs)
    {
        return sicm_arena_set_device_list(arena, devs);
    }

private:
    sicm_arena arena;
    bool cleanup;
};

template <class T, class U>
bool
operator==(SICMArenaAllocator<T> const&, SICMArenaAllocator<U> const&)
{
    return true;
}

template <class T, class U>
bool
operator!=(SICMArenaAllocator<T> const& x, SICMArenaAllocator<U> const& y)
{
    return !(x == y);
}

#endif
