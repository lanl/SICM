# SICM
Simplified Interface to Complex Memory

[![GitHub Actions](https://github.com/lanl/SICM/actions/workflows/sicm.yml/badge.svg)](https://github.com/lanl/SICM/actions)

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

The only dependencies that you will need for the low-level interface
are `libnuma` and `jemalloc`. We require that `jemalloc` be
configured with the `je_` prefix (using the `--with-jemalloc-prefix` flag).
`CMake` will use `pkg-config` to find `jemalloc`.

For the high-level interface, you need an installation of LLVM. LLVM 4.0 and
later have been tested, although 3.9 may possibly work. For the profiling, you
will also need an installation of `libpfm`, which is a small helper library for
`perf` that is available on most distributions.

Additionally, several other packages are required, and can be installed through a package manager:

### Binaries
- A modern C compiler
- A modern C++ compiler
- A modern Fortran compiler
- CMake 3.0+
- Make
- numactl
- automake + friends (if jemalloc needs to be built)

### Development Libraries
These packages are usually named `lib*-dev` or `lib*-devel`:

- numa

Additional packages are required for the high level interface:
- hwloc
- llvm
- omp (if OpenMP is not available by default on your compilers)
- pfm4

## Compilation
```
export PKG_CONFIG_PATH=<jemalloc prefix>/lib/pkgconfig:$PKG_CONFIG_PATH
mkdir build
cd build
cmake .. -DCMAKE_INSTALL_PREFIX=<prefix>
make
make install
```

## Low-Level API
| Function Name | Description |
|---------------|-------------|
| `sicm_init`  | Detects all memory devices on system, returns a list of them. |
| `sicm_fini`  | Frees up a device list and associated SICM data structures. |
| `sicm_find_device` | Return the first device that matches a given type and page size. |
| `sicm_device_alloc` | Allocates to a given device. |
| `sicm_device_free` | Frees memory on a device. |
| `sicm_can_place_exact` | Returns whether or not a device supports exact placement. |
| `sicm_device_alloc_exact` | Allocate memory on a device with an exact base address. |
| `sicm_numa_id` | Returns the NUMA ID that a device is on. |
| `sicm_device_page_size` | Returns the page size of a given device. |
| `sicm_device_eq` | Returns if two devices are equal or not. |
| `sicm_move`| Moves memory from one device to another. |
| `sicm_pin` | Pin the current process to a device's memory. |
| `sicm_capacity` | Returns the capacity of a given device. |
| `sicm_avail` | Returns the amount of memory available on a given device. |
| `sicm_model_distance` | Returns the distance of a given memory device. |
| `sicm_is_near` | Returns whether or not a given memory device is nearby the current NUMA node. |
| `sicm_latency` | Measures the latency of a memory device. |
| `sicm_bandwidth_linear2` | Measures a memory device's linear access bandwidth. |
| `sicm_bandwidth_random2` | Measures random access bandwidth of a memory device. |
| `sicm_bandwidth_linear3` | Measures the linear bandwidth of a memory device. |
| `sicm_bandwidth_random3` | Measures the random access bandwidth of a memory device. |

## Arena Allocator API
| Function Name | Description |
|---------------|-------------|
| `sicm_arenas_list` | List all arenas created in the arena allocator. |
| `sicm_arena_create` | Create a new arena on the given device. |
| `sicm_arena_destroy` | Frees up an arena, deleting all associated data structures. |
| `sicm_arena_set_default` | Sets an arena as the default for the current thread. |
| `sicm_arena_get_default` | Gets the default arena for the current thread. |
| `sicm_arena_get_device` | Gets the device for a given arena. |
| `sicm_arena_set_device` | Sets the memory device for a given arena. Moves all allocated memory already allocated to the arena. |
| `sicm_arena_size` | Gets the size of memory allocated to the given arena. |
| `sicm_arena_alloc` | Allocate to a given arena. |
| `sicm_arena_alloc_aligned` | Allocate aligned memory to a given arena. |
| `sicm_arena_realloc` | Resize allocated memory to a given arena. |
| `sicm_arena_lookup` | Returns which arena a given pointer belongs to. |

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

## Programming Practices
1. All blocks use curly braces
   - Even one-line blocks
2. Constants on the left side of `==`
   - `if(NULL == foo) { ...`
3. Functions with no arguments are `(void)`
4. No C++-style comments in C code
5. No GCC extensions except in GCC-only code
6. No C++ code in libraries
   - Discouraged in components
7. Always define preprocessor macros
   - Define logicals to 0 or 1 (vs. define or not define)
   - Use `#if FOO`, not `#ifdef FOO`
