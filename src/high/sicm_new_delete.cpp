#ifndef OP_NEW_DEL
#define OP_NEW_DEL

#include <cstdlib>
#include <new>
#include "sicm_high.h"

void  *operator new(std::size_t size) __attribute__((used)) __attribute__((noinline))  {
  return sh_alloc(0, size);
}
void  *operator new[](std::size_t size) __attribute__((used)) __attribute__((noinline))  {
  return sh_alloc(0, size);
}
void  *operator new(std::size_t size, const std::nothrow_t &) noexcept __attribute__((used)) __attribute__((noinline))  {
  return sh_alloc(0, size);
}
void  *operator new[](std::size_t size, const std::nothrow_t &) noexcept __attribute__((used)) __attribute__((noinline))  {
  return sh_alloc(0, size);
}

void  operator delete(void *ptr) noexcept __attribute__((used)) __attribute__((noinline))  { sh_free(ptr); }
void  operator delete[](void *ptr) noexcept __attribute__((used)) __attribute__((noinline))  { sh_free(ptr); }
void  operator delete(void *ptr, const std::nothrow_t &) noexcept __attribute__((used)) __attribute__((noinline))  { sh_free(ptr); }
void  operator delete[](void *ptr, const std::nothrow_t &) noexcept __attribute__((used)) __attribute__((noinline))  { sh_free(ptr); }

#endif
