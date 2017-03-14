#pragma once
extern "C" {
  #include "sg.h"
}
#include <limits>

class SGExact {
  public:
    void* operator new(size_t);
    void operator delete(void*);
};

class SGPerf {
  public:
    void* operator new(size_t);
    void operator delete(void*);
};

class SGCap {
  public:
    void* operator new(size_t);
    void operator delete(void*);
};

template <class T>
class SGExactAllocator {
public:
  typedef T        value_type;
  typedef T*       pointer;
  typedef const T* const_pointer;
  typedef T&       reference;
  typedef const T& const_reference;
  typedef std::size_t    size_type;
  typedef std::ptrdiff_t difference_type;

  template <class U>
  struct rebind {
     typedef SGExactAllocator<U> other;
  };

  pointer address(reference value) const {
    return &value;
  }
  const_pointer address(const_reference value) const {
    return &value;
  }

  SGExactAllocator() throw() {}
  SGExactAllocator(const SGExactAllocator& x) throw() {}
  template <class U> SGExactAllocator(const SGExactAllocator<U>& x) throw() {}
  ~SGExactAllocator() throw() {}

  size_type max_size() const throw() {
     return std::numeric_limits<std::size_t>::max() / sizeof(T);
  }

  pointer allocate(size_type num, const void* = 0) {
    pointer mem = (pointer)sg_alloc_exact(num * sizeof(T));
    if(mem == NULL)
      throw "sg allocation fail (probably not enough memory)";
    return mem;
  }

  void construct (pointer p, const T& value) {
     new((void*)p)T(value);
  }

  void destroy (pointer p) {
     p->~T();
  }

  void deallocate (pointer p, size_type num) {
    sg_free(p);

  }
};

template <class T>
class SGPerfAllocator {
public:
  typedef T        value_type;
  typedef T*       pointer;
  typedef const T* const_pointer;
  typedef T&       reference;
  typedef const T& const_reference;
  typedef std::size_t    size_type;
  typedef std::ptrdiff_t difference_type;

  template <class U>
  struct rebind {
     typedef SGPerfAllocator<U> other;
  };

  pointer address(reference value) const {
    return &value;
  }
  const_pointer address(const_reference value) const {
    return &value;
  }

  SGPerfAllocator() throw() {}
  SGPerfAllocator(const SGPerfAllocator& x) throw() {}
  template <class U> SGPerfAllocator(const SGPerfAllocator<U>& x) throw() {}
  ~SGPerfAllocator() throw() {}

  size_type max_size() const throw() {
     return std::numeric_limits<std::size_t>::max() / sizeof(T);
  }

  pointer allocate(size_type num, const void* = 0) {
    pointer mem = (pointer)sg_alloc_perf(num * sizeof(T));
    if(mem == NULL)
      throw "sg allocation fail (probably not enough memory)";
    return mem;
  }

  void construct (pointer p, const T& value) {
     new((void*)p)T(value);
  }

  void destroy (pointer p) {
     p->~T();
  }

  void deallocate (pointer p, size_type num) {
    sg_free(p);

  }
};

template <class T>
class SGCapAllocator {
public:
  typedef T        value_type;
  typedef T*       pointer;
  typedef const T* const_pointer;
  typedef T&       reference;
  typedef const T& const_reference;
  typedef std::size_t    size_type;
  typedef std::ptrdiff_t difference_type;

  template <class U>
  struct rebind {
     typedef SGCapAllocator<U> other;
  };

  pointer address(reference value) const {
    return &value;
  }
  const_pointer address(const_reference value) const {
    return &value;
  }

  SGCapAllocator() throw() {}
  SGCapAllocator(const SGCapAllocator& x) throw() {}
  template <class U> SGCapAllocator(const SGCapAllocator<U>& x) throw() {}
  ~SGCapAllocator() throw() {}

  size_type max_size() const throw() {
     return std::numeric_limits<std::size_t>::max() / sizeof(T);
  }

  pointer allocate(size_type num, const void* = 0) {
    pointer mem = (pointer)sg_alloc_cap(num * sizeof(T));
    if(mem == NULL)
      throw "sg allocation fail (probably not enough memory)";
    return mem;
  }

  void construct (pointer p, const T& value) {
     new((void*)p)T(value);
  }

  void destroy (pointer p) {
     p->~T();
  }

  void deallocate (pointer p, size_type num) {
    sg_free(p);

  }
};
