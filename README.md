# SICM
Simplified Interface to Complex Memory

[![Build Status](https://travis-ci.org/lanl/SICM.svg?branch=master)](https://travis-ci.org/lanl/SICM)

## Introduction
This project is split into two interfaces: `low` and `high`.

The `low` interface provides a minimal interface for application wanting to
manage their own memory on heterogeneous memory tiers. It also provides an
arena allocator that application developers can use to create `jemalloc` arenas
on different memory tiers and allocate to those tiers.

The `high` interface attempts to automatically manage the memory tiers for the
application. It provides an LLVM compiler pass (and compiler wrappers) to
automatically transform applications to make the appropriate `high` interface
calls, as well as a runtime library which provides profiling for the
application.  The profiling is currently meant to be used offline; that is,
after enabling the profiling for an application run, the results are printed
out at the end of the run, and that information must be fed into a second run
to make use of it. An online approach is planned.

## Dependencies

The only dependency that you will need for the low-level interface is
`jemalloc`. The build system will automatically look in `/usr` for this
dependency, but if you want to provide a custom location, you can pass the
`--with-jemalloc` argument to `configure`.

For the high-level interface, you need an installation of LLVM. LLVM 4.0 and
later have been tested, although 3.9 may possibly work. For the profiling, you
will also need an installation of `libpfm`, which is a small helper library for
`perf` that is available on most distributions. These two dependencies are
found automatically, but again, the user can specify `--with-llvm=` and
`--with-libpfm` to use a custom location.

Both `jemalloc` and `llvm` can be installed by simply running
`./install-deps.sh`, which downloads and compiles them in `build_deps/`, then
installs them in `deps/`. If you do not want to install `llvm`, use the
`--no-llvm` argument; you can also use `--no-jemalloc`.

As an example for using your package manager to install the dependencies,
Ubuntu Trusty requires the following packages.  You can replace the GCC version
with whichever you want, or use Clang instead.
```
g++-8
gfortran-8
numactl
libnuma-dev
wget
libpfm4
libpfm4-dev
llvm-3.9-dev
libiomp-dev
```
If you have a version of LLVM newer than 3.9, you can likely omit `libiomp-dev`
in exchange for the builtin OpenMP implementation.

## Compilation
To compile and install on a new system, run
```
./autogen.sh
./configure --prefix=[DIR]
make
make install
```

## Low-Level API
- `sicm_device_list sicm_init()`
  - Detects all memory devices on system, returns a list of them.
- `void sicm_fini(sicm_device_list *)`
  - Frees up a device list and associated SICM data structures.
- `sicm_device *sicm_find_device(sicm_device_list *devs, const sicm_device_tag type, const int page_size, sicm_device *old)`
  - Return the first device that matches a given type and page size.
- `void *sicm_device_alloc(struct sicm_device *device, size_t sz)`
  - Allocates to a given device.
- `void sicm_device_free(struct sicm_device *device, void *ptr, size_t size)`
  - Frees memory on a device.
- `int sicm_can_place_exact(struct sicm_device* device)`
  - Returns whether or not a device supports exact placement.
- `void* sicm_device_alloc_exact(struct sicm_device* device, void* base, size_t size)`
  - Allocate memory on a device with an exact base address.
- `int sicm_numa_id(sicm_device* device);`
  - Returns the NUMA ID that a device is on.
- `int sicm_device_page_size(sicm_device* device)`
  - Returns the page size of a given device.
- `int sicm_device_eq(sicm_device* dev1, sicm_device* dev2)`
  - Returns if two devices are equal or not.
- `int sicm_move(sicm_device* src, sicm_device* dst, void* ptr, size_t size)`
  - Moves memory from one device to another.
- `int sicm_pin(sicm_device* device)`
  - Pin the current process to a device's memory.
- `size_t sicm_capacity(sicm_device* device)`
  - Returns the capacity of a given device.
- `size_t sicm_avail(sicm_device* device)`
  - Returns the amount of memory available on a given device.
- `int sicm_model_distance(sicm_device* device)`
  - Returns the distance of a given memory device.
- `int sicm_is_near(sicm_device* device)`
  - Returns whether or not a given memory device is nearby the current NUMA node.
- `void sicm_latency(sicm_device* device, size_t size, int iter, struct sicm_timing* res)`
  - Measures the latency of a memory device.
- `size_t sicm_bandwidth_linear2(sicm_device* device, size_t size, size_t (*kernel)(double* a, double* b, size_t size))`
  - Measures a memory device's linear access bandwidth.
- `size_t sicm_bandwidth_random2(sicm_device* device, size_t size, size_t (*kernel)(double* a, double* b, size_t* indexes, size_t size))`
  - Measures random access bandwidth of a memory device.
- `size_t sicm_bandwidth_linear3(sicm_device* device, size_t size, size_t (*kernel)(double* a, double* b, double* c, size_t size))`
  - Measures the linear bandwidth of a memory device.
- `size_t sicm_bandwidth_random3(sicm_device* device, size_t size, size_t (*kernel)(double* a, double* b, double* c, size_t* indexes, size_t size))`
  - Measures the random access bandwidth of a memory device.

## Arena Allocator API
- `sicm_arena_list *sicm_arenas_list()`
  - List all arenas created in the arena allocator.
- `sicm_arena sicm_arena_create(size_t maxsize, sicm_device *dev)`
  - Create a new arena on the given device.
- `void sicm_arena_destroy(sicm_arena arena)`
  - Frees up an arena, deleting all associated data structures.
- `void sicm_arena_set_default(sicm_arena sa)`
  - Sets an arena as the default for the current thread.
- `sicm_arena sicm_arena_get_default(void)`
  - Gets the default arena for the current thread.
- `sicm_device *sicm_arena_get_device(sicm_arena sa)`
  - Gets the device for a given arena.
- `int sicm_arena_set_device(sicm_arena sa, sicm_device *dev)`
  - Sets the memory device for a given arena. Moves all allocated memory already allocated to the arena.
- `size_t sicm_arena_size(sicm_arena sa)`
  - Gets the size of memory allocated to the given arena.
- `void *sicm_arena_alloc(sicm_arena sa, size_t sz)`
  - Allocate to a given arena.
- `void *sicm_arena_alloc_aligned(sicm_arena sa, size_t sz, size_t align)`
  - Allocate aligned memory to a given arena.
- `void *sicm_arena_realloc(sicm_arena sa, void *ptr, size_t sz)`
  - Resize allocated memory to a given arena.
- `sicm_arena sicm_arena_lookup(void *ptr)`
  - Returns which arena a given pointer belongs to.

## High-Level Interface
The high-level interface is normally used with the compiler wrappers located in
`bin/`. Users should use these wrappers to compile their applications, and a
compiler pass will automatically transform the code so that it calls the
high-level interface with the appropriate arguments, including initialization,
destruction, and the proper allocation functions. Assuming the high-level
interface is linked to the application as a shared library, it automatically
initializes itself.  All heap allocation routines are replaced by calls to
`void* sh_alloc(int id, size_t sz)`, which associates an ID with a given
allocation and allocates the memory into an arena with other allocations of
that ID.
