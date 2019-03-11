#ifndef OP_NEW_DEL
#define OP_NEW_DEL

#include <cstdlib>
#include <new>
#include "sicm_high.h"

/* Never inline these */
void  *operator new(std::size_t size) __attribute__((used)) __attribute__((noinline));
void  *operator new[](std::size_t size) __attribute__((used)) __attribute__((noinline));
void  *operator new(std::size_t size, const std::nothrow_t &) noexcept __attribute__((used)) __attribute__((noinline));
void  *operator new[](std::size_t size, const std::nothrow_t &) noexcept __attribute__((used)) __attribute__((noinline));
void  operator delete(void *ptr) noexcept __attribute__((used)) __attribute__((noinline));
void  operator delete[](void *ptr) noexcept __attribute__((used)) __attribute__((noinline));
void  operator delete(void *ptr, const std::nothrow_t &) noexcept __attribute__((used)) __attribute__((noinline));
void  operator delete[](void *ptr, const std::nothrow_t &) noexcept __attribute__((used)) __attribute__((noinline));

/* Call sh_alloc from all of these */
void  *operator new(std::size_t size){
  return sh_alloc(0, size);
}
void  *operator new[](std::size_t size){
  return sh_alloc(0, size);
}
void  *operator new(std::size_t size, const std::nothrow_t &){
  return sh_alloc(0, size);
}
void  *operator new[](std::size_t size, const std::nothrow_t &){
  return sh_alloc(0, size);
}
void  operator delete(void *ptr){
  sh_free(ptr);
}
void  operator delete[](void *ptr){
  sh_free(ptr);
}
void  operator delete(void *ptr, const std::nothrow_t &){
  sh_free(ptr);
}
void  operator delete[](void *ptr, const std::nothrow_t &){
  sh_free(ptr);
}

#endif
