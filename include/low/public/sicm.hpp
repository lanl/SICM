#ifndef SICM_CXX_ALLOCATOR
#define SICM_CXX_ALLOCATOR

// Modified from Howard Hinnant's Allocator Boilerplate Templates
// https://howardhinnant.github.io/allocator_boilerplate.html
// which is licesned under CC BY 4.0
// (https://creativecommons.org/licenses/by/4.0/)

#if __cplusplus < 201103L     // C++03 and backward
#include "sicm-arena-03.hpp"
#include "sicm-device-03.hpp"
#elif __cplusplus >= 201103L  // C++11 and forward
#include "sicm-arena-11.hpp"
#include "sicm-device-11.hpp"
#endif

#endif
