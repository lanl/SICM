#pragma once
extern "C" {
  #include "sicm_low.h"
  #include <pthread.h>
}
#include <limits>

pthread_key_t sicm_current_device, sicm_current_size;

class SicmClass {
  public:
    void* operator new(size_t);
    void operator delete(void*);
    struct sicm_device* sicm_dev;
    size_t sicm_sz;
};

template <class T>
class SicmAllocator {
public:
  typedef T        value_type;
  typedef T*       pointer;
  typedef const T* const_pointer;
  typedef T&       reference;
  typedef const T& const_reference;
  typedef std::size_t    size_type;
  typedef std::ptrdiff_t difference_type;

  struct sicm_device* sicm_dev;

  template <class U>
  struct rebind {
     typedef SicmAllocator<U> other;
  };

  pointer address(reference value) const {
    return &value;
  }
  const_pointer address(const_reference value) const {
    return &value;
  }

  SicmAllocator() throw() {
    this->sicm_dev =
      (struct sicm_device*)pthread_getspecific(sicm_current_device);
  }
  SicmAllocator(const SicmAllocator& x) throw() {
    this->sicm_dev = x->sicm_dev;
  }
  template <class U> SicmAllocator(const SicmAllocator<U>& x) throw() {
    this->sicm_dev = x->sicm_dev;
  }
  ~SicmAllocator() throw() {}

  size_type max_size() const throw() {
     return std::numeric_limits<std::size_t>::max() / sizeof(T);
  }

  pointer allocate(size_type num, const void* = 0) {
    pointer mem;
    if(this->sicm_dev == NULL)
      mem = (pointer)malloc(num * sizeof(T));
    else
      mem = (pointer)sicm_device_alloc(this->sicm_dev, num * sizeof(T));
    if(mem == NULL)
      throw "sicm_stl allocation fail (probably not enough memory)";
    return mem;
  }

  void construct (pointer p, const T& value) {
     new((void*)p)T(value);
  }

  void destroy (pointer p) {
     p->~T();
  }

  void deallocate (pointer p, size_type num) {
    if(this->sicm_dev == NULL)
      free(p);
    else
      sicm_device_free(this->sicm_dev, p, num * sizeof(T));
  }
};

template <class T1, class T2>
bool operator== (const SicmAllocator<T1>&, const SicmAllocator<T2>&) throw() {
  return true;
}
template <class T1, class T2>
bool operator!= (const SicmAllocator<T1>&, const SicmAllocator<T2>&) throw() {
  return false;
}

struct sicm_device_list sicm_init_cpp();

#define sicm_new(device, type, name, constructor_args) \
  pthread_setspecific(sicm_current_device, device); \
  type* name = new type constructor_args; \
  name->sicm_dev = device; \
  name->sicm_sz = (size_t)pthread_getspecific(sicm_current_size); \
  pthread_setspecific(sicm_current_device, NULL); \
  pthread_setspecific(sicm_current_size, NULL);

#define sicm_delete(name) \
  pthread_setspecific(sicm_current_device, (const void*)name->sicm_dev); \
  pthread_setspecific(sicm_current_size, (const void*)name->sicm_sz); \
  delete name; \
  pthread_setspecific(sicm_current_device, NULL); \
  pthread_setspecific(sicm_current_size, NULL);

#define sicm_stl(device, container, type, name) \
  pthread_setspecific(sicm_current_device, device); \
  container<type, SicmAllocator<type> > name;
