#ifndef SICM_ARENA_CXX_11_ALLOCATOR
#define SICM_ARENA_CXX_11_ALLOCATOR

#include "sicm_low.h"

template <class T>
class SICMArenaAllocator
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
    friend class SICMArenaAllocator;

    SICMArenaAllocator(sicm_arena a = ARENA_DEFAULT) noexcept
        : arena(a)
    {}

    SICMArenaAllocator(sicm_device *dev,
                       const std::size_t max_size = 0) noexcept
        : SICMArenaAllocator(&dev, 1, max_size)
    {}

    SICMArenaAllocator(sicm_device **dev_array, const std::size_t count,
                       const std::size_t max_size = 0) noexcept
        : SICMArenaAllocator()
    {
        sicm_device_list devs = { .count = count, .arenas = dev_array };
        arena = sicm_arena_create(max_size, 0, &devs);
    }

    SICMArenaAllocator(sicm_device_list *dev_list,
                       const std::size_t max_size = 0) noexcept
        : arena(sicm_arena_create(max_size, 0, dev_list))
    {}

    template <class U> SICMArenaAllocator(SICMArenaAllocator<U> const& u) noexcept
        : arena(u.arena)
    {}

    value_type*  // Use pointer if pointer is not a value_type*
    allocate(std::size_t n)
    {
        return static_cast<value_type*>(sicm_arena_alloc(arena, n*sizeof(value_type)));
    }

    void
    deallocate(value_type* p, std::size_t) noexcept  // Use pointer if pointer is not a value_type*
    {
        sicm_free(p);
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
};

template <class T, class U>
bool
operator==(SICMArenaAllocator<T> const&, SICMArenaAllocator<U> const&) noexcept
{
    return true;
}

template <class T, class U>
bool
operator!=(SICMArenaAllocator<T> const& x, SICMArenaAllocator<U> const& y) noexcept
{
    return !(x == y);
}

#endif
