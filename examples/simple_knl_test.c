#include "sg.h"
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>

#define TEST_LEN 1073741824

int main() {
  struct timespec start, end;
  size_t size = 1024;
  double *a, *b, *c;
  size_t i, m;
  double delta;
  sg_init(0);
  while(size < 1073741824) {
    a = malloc(size * sizeof(double));
    b = malloc(size * sizeof(double));
    c = malloc(size * sizeof(double));
    #pragma omp parallel for
    for(i = 0; i < size; i++) {
      a[i] = 1;
      b[i] = 2;
      c[i] = 3;
    }
    clock_gettime(CLOCK_MONOTONIC_RAW, &start);
    #pragma omp parallel for
    for(i = 0; i < TEST_LEN; i++) {
      m = i % size;
      a[m] = b[m] + 5 * c[m];
    }
    clock_gettime(CLOCK_MONOTONIC_RAW, &end);
    delta = (end.tv_sec - start.tv_sec) * 1000000 + (end.tv_nsec - start.tv_nsec) / 1000;
    printf("size %lu, malloc, bw: %f\n", size, (double)TEST_LEN * 3 * sizeof(double) / (1000 * delta));
    free(a);
    free(b);
    free(c);

    a = sg_alloc_perf(size * sizeof(double));
    b = sg_alloc_perf(size * sizeof(double));
    c = sg_alloc_perf(size * sizeof(double));
    #pragma omp parallel for
    for(i = 0; i < size; i++) {
      a[i] = 1;
      b[i] = 2;
      c[i] = 3;
    }
    clock_gettime(CLOCK_MONOTONIC_RAW, &start);
    #pragma omp parallel for
    for(i = 0; i < TEST_LEN; i++) {
      m = i % size;
      a[m] = b[m] + 5 * c[m];
    }
    clock_gettime(CLOCK_MONOTONIC_RAW, &end);
    delta = (end.tv_sec - start.tv_sec) * 1000000 + (end.tv_nsec - start.tv_nsec) / 1000;
    printf("size %lu, sg, bw: %f\n", size, (double)TEST_LEN * 3 * sizeof(double) / (1000 * delta));
    sg_free(a);
    sg_free(b);
    sg_free(c);

    size *= 2;
  }
}
