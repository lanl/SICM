#ifndef __SICMIMPL_H
#define __SICMIMPL_H

#include <sys/types.h>

#include <jemalloc/jemalloc.h>
#include "sicm_extent_arr.h"
//MKL
//#include "/usr/include/asm-generic/mman-common.h"
/// Linear-feedback shift register PRNG algorithm.
/**
 * @param[in,out] n The previous random number/initial seed. This should
 * be unsigned.
 *
 * This is obviously not a cryptographic algorithm, or even very good
 * for intense random number exercises, but we just want something
 * that'll produce cache misses and avoid prefetching. This is done for
 * speed (to avoid polluting timings) and purity (for thread safety).
 * This function must be seeded manually. time(NULL) can be used, or if
 * multiple threads will be launched nearby in time, you can the address
 * of a local variable.
 *
 * The "magic number" 0xd0000001u is the maximum-period internal XOR
 * value for the 64-bit Galois LSFR.
 */
#define sicm_rand(n) \
  n = (n >> 1) ^ (unsigned int)((0 - (n & 1u)) & 0xd0000001u)

/// FNV-1A hash algorithm.
/**
 * @param[in] n The value to be hashed.
 * @return The hash of n.
 *
 * This is a simple but reasonably effective hash algorithm. It's mainly
 * useful for generating random index arrays, particularly if the index
 * array is being generated in parallel. So this is preferred if you
 * have a large amount of input values to transform, while if you have a
 * single value that you want to process on repeatedly, sicm_rand is
 * preferred. Of course, this is not a cryptographic algorithm.
 *
 * The "magic numbers" were chosen by the original authors (Fowler,
 * Noll, and Vo) for good hashing behavior. These numbers are
 * specifically tailored to 64-bit integers.
 */
#define sicm_hash(n) ((0xcbf29ce484222325 ^ (n)) * 0x100000001b3)

/// Ceiling of integer division.
/**
 * @param[in] n Numerator (dividend).
 * @param[in] d Denominator (divisor).
 * @return Quotient, rounded toward positive infinity.
 *
 * Ordinary integer division implicitly floors, so 1 / 2 = 0. This
 * instead returns the ceiling, so 1 / 2 = 1. This works by just
 * adding 1 if the division produces a nonzero remainder.
 */
#define sicm_div_ceil(n, d) ((n) / (d) + ((n) % (d) ? 1 : 0))

/// System page size in KiB.
/**
 * This variable is initialized by sicm_init(), so don't use it before
 * then.
 */
extern int normal_page_size;

#ifdef __GNUC__
#define member_type(type, member) __typeof__ (((type *)0)->member)
#else
#define member_type(type, member) const void
#endif

#ifndef offsetof
#define offsetof(TYPE, MEMBER) ((size_t) &((TYPE *)0)->MEMBER)
#endif

#define container_of(ptr, type, member) ((type *)( \
    (char *)(member_type(type, member) *){ ptr } - offsetof(type, member)))

typedef struct sarena sarena;


/* Stores information about a jemalloc arena */
struct sarena {
    pthread_mutexattr_t attr;
    pthread_mutex_t*    mutex;
    sicm_device*        dev;
    size_t              maxsize;
    size_t              size;
    size_t              pagesize;
    int                 numaid;
    sarena*             next;

    /* jemalloc related */
    unsigned            arena_ind;
    extent_hooks_t      hooks;

    /* jemalloc extent ranges */
    extent_arr*         extents;

    int                 err;
    int                 fd;
    int                 user;
};

extern sarena *sarena_ptr2sarena(void *ptr);
extern int sicm_arena_init(void);

/* Set by the user, called whenever an extent is allocated */
extern void (*sicm_extent_alloc_callback)(void *start, void *end);

#endif
