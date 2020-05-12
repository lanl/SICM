/*
 * sicm_rdspy.h
 * Delcarations for rdspy allocation site profiling
 * Brandon Kammerdiener
 * December, 2018
 */

#ifndef _SICM_RDSPY_H_
#define _SICM_RDSPY_H_

#define READ_TIMES_BUCKET_SIZE (8)
#define READ_TIMES_MAX (80)
#define READ_TIMES_NBUCKETS (READ_TIMES_MAX / READ_TIMES_BUCKET_SIZE)

#include <stdint.h>
#include <stdlib.h>

typedef unsigned long long hist_t[READ_TIMES_NBUCKETS];

typedef struct {
    void    *addr;
    uint64_t ticks;
} AddrTicks;

typedef struct {
    AddrTicks *list,
              *top;
    hist_t    *histograms;
} ThreadReadsInfo;

void ThreadReadsInfo_init(ThreadReadsInfo*);
void ThreadReadsInfo_finish(ThreadReadsInfo*);
void ThreadReadsInfo_add(ThreadReadsInfo*, void*, uint64_t);
void ThreadReadsInfo_flush(ThreadReadsInfo*);

typedef struct {
    hist_t *histograms;
} SiteReadsAgg;

void SiteReadsAgg_init(SiteReadsAgg*);
void SiteReadsAgg_finish(SiteReadsAgg*);
void SiteReadsAgg_give_histogram(SiteReadsAgg*, ThreadReadsInfo*);

void sh_rdspy_alloc(void *ptr, size_t sz, unsigned id);
void sh_rdspy_realloc(void *old, void *ptr, size_t sz, unsigned id);
void sh_rdspy_free(void *ptr);
unsigned long long sh_read(void * ptr, uint64_t beg, uint64_t end);
void sh_rdspy_init(int _max_threads, int _num_static_sites);
void sh_rdspy_terminate();

#endif
