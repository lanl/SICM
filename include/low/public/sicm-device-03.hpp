#ifndef SICM_DEVICE_CXX_03_ALLOCATOR
#define SICM_DEVICE_CXX_03_ALLOCATOR

#include <limits>
#include <memory>
#include <pthread.h>

#include "sicm_low.h"

template <class T> class SICMDeviceAllocator;

template <>
class SICMDeviceAllocator<void>
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
        typedef SICMDeviceAllocator<U> other;
    };
};

template <class T>
class SICMDeviceAllocator
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
        typedef SICMDeviceAllocator<U> other;
    };

    template <class U>
    friend class SICMDeviceAllocator;

    SICMDeviceAllocator(sicm_device *dev = sicm_default_device(-1)) throw() {
        sicm_dev = dev;
    }

    template <class U> SICMDeviceAllocator(SICMDeviceAllocator<U> const& u) throw() {
        sicm_dev = u.sicm_dev;
    }

    pointer
    allocate(size_type n, SICMDeviceAllocator<void>::const_pointer = 0)
    {
        void *mem = NULL;

        if (sicm_dev) {
            if (!(mem = sicm_device_alloc(sicm_dev, n * sizeof(value_type)))) {
                throw std::bad_alloc();
            }
        }
        else {
            mem = ::operator new (n * sizeof(value_type));
        }

        return (pointer) mem;
    }

    void
    deallocate(pointer p, size_type n)
    {
        if (sicm_dev) {
            sicm_device_free(sicm_dev, p, n * sizeof(value_type));
        }
        else {
            ::operator delete(p);
        }
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

private:
    sicm_device *sicm_dev;
};

template <class T, class U>
bool
operator==(SICMDeviceAllocator<T> const&, SICMDeviceAllocator<U> const&)
{
    return true;
}

template <class T, class U>
bool
operator!=(SICMDeviceAllocator<T> const& x, SICMDeviceAllocator<U> const& y)
{
    return !(x == y);
}

#endif
