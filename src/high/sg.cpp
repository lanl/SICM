#include "sg.hpp"

void* SGExact::operator new(size_t sz) {
  void* mem = sg_alloc_exact(sz);
  if(mem == NULL)
    throw "sg allocation fail (probably not enough memory)";
  return mem;
}

void SGExact::operator delete(void* mem) {
  sg_free(mem);
}

void* SGPerf::operator new(size_t sz) {
  void* mem = sg_alloc_perf(sz);
  if(mem == NULL)
    throw "sg allocation fail (probably not enough memory)";
  return mem;
}

void SGPerf::operator delete(void* mem) {
  sg_free(mem);
}

void* SGCap::operator new(size_t sz) {
  void* mem = sg_alloc_cap(sz);
  if(mem == NULL)
    throw "sg allocation fail (probably not enough memory)";
  return mem;
}

void SGCap::operator delete(void* mem) {
  sg_free(mem);
}
