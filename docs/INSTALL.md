# Installation

## General Notes

You don't need to compile both interfaces. By default, the CMake build system
will compile both, but you can disable the high-level interface by passing
`-DSICM_BUILD_HIGH_LEVEL=False` to CMake.

## Low-Level Interface Dependencies

There are three dependencies that you will need to compile the low-level interface:
1. `jemalloc`
2. `libnuma`
3. `pthreads`

If CMake is unable to find `jemalloc`, `cmake/Modules/FindJemalloc.cmake` uses the variable
`JEMALLOC_ROOT` for guidance. To set this variable, simply pass
`-DJEMALLOC_ROOT=[path]` to specify the path in which you have installed
`jemalloc`.

## High-Level Interface Dependencies

In addition to the low-level interface and its dependencies, the high-level interface depends on:
1. LLVM
2. `libpfm4`

For `libpfm4`, we provide a CMake module in `cmake/Modules/FindLibPfm4.cmake`. You can pass
`LIBPFM_INSTALL=[path]` to CMake to steer it toward a specific installation. For LLVM, however,
we rely on the system-installed `LLVMConfig.cmake`.

## Compilation

This is the general idiom for compiling a project with a CMake-based build system, and is
one example of how you might want to compile it.

```
mkdir build
cd build
cmake -DCMAKE_INSTALL_PREFIX=<prefix> \
      -DJEMALLOC_ROOT=<jemalloc_prefix> \
      -DLIBPFM_INSTALL=<libpfm_prefix> \
      -DSICM_BUILD_HIGH_LEVEL=True \
      ..
make
make install
```
