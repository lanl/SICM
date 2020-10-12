#ifndef SICM_CXX_11_ALLOCATOR
#define SICM_CXX_11_ALLOCATOR

#include "sicm_low.h"

template <class T>
class SICMAllocator
{
public:
    using value_type    = T;

//     using pointer       = value_type*;
//     using const_pointer = typename std::pointer_traits<pointer>::template
//                                                     rebind<value_type const>;
//     using void_pointer       = typename std::pointer_traits<pointer>::template
//                                                           rebind<void>;
//     using const_void_pointer = typename std::pointer_traits<pointer>::template
//                                                           rebind<const void>;

//     using difference_type = typename std::pointer_traits<pointer>::difference_type;
//     using size_type       = std::make_unsigned_t<difference_type>;

//     template <class U> struct rebind {typedef allocator<U> other;};

    template <class U>
    friend class SICMAllocator;

    SICMAllocator(sicm_device *dev = sicm_default_device(-1)) noexcept
        : sicm_dev(dev)
    {}

    template <class U> SICMAllocator(SICMAllocator<U> const& u) noexcept
        : sicm_dev(u.sicm_dev)
    {}

    value_type*  // Use pointer if pointer is not a value_type*
    allocate(std::size_t n)
    {
        return static_cast<value_type*>(sicm_device_alloc(sicm_dev, n*sizeof(value_type)));
    }

    void
    deallocate(value_type* p, std::size_t n) noexcept  // Use pointer if pointer is not a value_type*
    {
        sicm_device_free(sicm_dev, p, n * sizeof(value_type));
    }

//     value_type*
//     allocate(std::size_t n, const_void_pointer)
//     {
//         return allocate(n);
//     }

//     template <class U, class ...Args>
//     void
//     construct(U* p, Args&& ...args)
//     {
//         ::new(p) U(std::forward<Args>(args)...);
//     }

//     template <class U>
//     void
//     destroy(U* p) noexcept
//     {
//         p->~U();
//     }

//     std::size_t
//     max_size() const noexcept
//     {
//         return std::numeric_limits<size_type>::max();
//     }

//     allocator
//     select_on_container_copy_construction() const
//     {
//         return *this;
//     }

//     using propagate_on_container_copy_assignment = std::false_type;
//     using propagate_on_container_move_assignment = std::false_type;
//     using propagate_on_container_swap            = std::false_type;
//     using is_always_equal                        = std::is_empty<allocator>;

private:
    sicm_device *sicm_dev;
};

template <class T, class U>
bool
operator==(SICMAllocator<T> const&, SICMAllocator<U> const&) noexcept
{
    return true;
}

template <class T, class U>
bool
operator!=(SICMAllocator<T> const& x, SICMAllocator<U> const& y) noexcept
{
    return !(x == y);
}

#endif
