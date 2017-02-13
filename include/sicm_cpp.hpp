#pragma once
extern "C" {
  #include "sicm_low.h"
  #include <pthread.h>
}

pthread_key_t sicm_current_device, sicm_current_size;

class SicmClass {
  public:
    void* operator new(size_t);
    void operator delete(void*);
    struct sicm_device* sicm_dev;
    size_t sicm_sz;
};

struct sicm_device_list sicm_init_cpp();

#define sicm_new(d, t, n, a) \
  pthread_setspecific(sicm_current_device, d); \
  t* n = new t a; \
  n->sicm_dev = d; \
  n->sicm_sz = (size_t)pthread_getspecific(sicm_current_size); \
  pthread_setspecific(sicm_current_device, NULL); \
  pthread_setspecific(sicm_current_size, NULL);

#define sicm_delete(n) \
  pthread_setspecific(sicm_current_device, (const void*)n->sicm_dev); \
  pthread_setspecific(sicm_current_size, (const void*)n->sicm_sz); \
  delete n; \
  pthread_setspecific(sicm_current_device, NULL); \
  pthread_setspecific(sicm_current_size, NULL);
