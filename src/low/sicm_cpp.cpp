#include "sicm_cpp.hpp"

void* SicmClass::operator new(size_t sz) {
  struct sicm_device* device =
    (struct sicm_device*)pthread_getspecific(sicm_current_device);
  void* mem;
  if(device == NULL)
    mem = malloc(sz);
  else {
    mem = sicm_device_alloc(device, sz);
    pthread_setspecific(sicm_current_size, (const void*)sz);
  }
  if(mem == NULL)
    throw "sicm_new allocation fail (probably not enough memory)";
  return mem;
}

void SicmClass::operator delete(void* mem) {
  struct sicm_device* device =
    (struct sicm_device*)pthread_getspecific(sicm_current_device);
  size_t sz = (size_t)pthread_getspecific(sicm_current_size);
  if(device == NULL)
    free(mem);
  else
    sicm_device_free(device, mem, sz);
}

struct sicm_device_list sicm_init_cpp() {
  pthread_key_create(&sicm_current_device, NULL);
  pthread_key_create(&sicm_current_size, NULL);
  return sicm_init();
}
