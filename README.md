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

`jemalloc` and `llvm` can be installed by running `install_deps.sh` with
the appropriate command line options. In order to assist with the automation
of building and running SNAP, `OpenMPI-3.1.1` can also be installed through
this script.

As an example for using your package manager to install the dependencies,
Ubuntu Trusty requires the following packages.  You can replace the GCC version
with whichever you want, or use Clang instead.
```
g++-8
gfortran-8
libhwloc-dev
libiomp-dev
libnuma-dev
libpfm4
libpfm4-dev
llvm-3.9-dev
numactl
```
If you have a version of LLVM newer than 3.9, you can likely omit `libiomp-dev`
in exchange for the builtin OpenMP implementation.

On Debian Stable (9.5.0 at time of writing), the required packages (for both
low- and high-level interfaces) are:
```
autoconf
automake
g++
gcc
gfortran
git
libnuma-dev
libpfm4-dev
libtool
libtool-bin
llvm-4.0
llvm-4.0-dev
make
```

## Compilation
In general, to compile and install on a new system, run
```
./autogen.sh
./configure --prefix=[DIR]
make
make install
```

On a fresh Debian Stable (9.5.0) system, SICM can easily be installed with these commands:
```
sudo apt-get install git autoconf automake make gcc libpfm4-dev llvm-4.0-dev llvm-4.0 libtool g++ libnuma-dev libtool-bin gfortran
git clone https://github.com/lanl/SICM.git
cd SICM
./install_deps.sh --no-llvm
./autogen.sh
./configure --prefix=$(readlink -f ./deps) --with-jemalloc=$(readlink -f ./deps) --with-llvm=$(llvm-config-4.0 --prefix)
make -j$(nproc --all)
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
