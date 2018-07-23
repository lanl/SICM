#include "sg.h"
#include <stdio.h>

void sg_init_wrap_(int* id) {
  sg_init(*id);
}

void sg_alloc_exact_wrap_(size_t* sz, void** ptr) {
  *ptr = sg_alloc_exact(*sz);
}

void sg_alloc_cap_wrap_(size_t* sz, void** ptr) {
  *ptr = sg_alloc_cap(*sz);
}

void sg_alloc_perf_wrap_(size_t* sz, void** ptr) {
  *ptr = sg_alloc_perf(*sz);
}

void sg_free_wrap_(void** ptr) {
  sg_free(*ptr);
}
