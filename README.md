# SICM
Simplified Interface to Complex Memory

This project is split into two interfaces: `low` and `high`.

The `low` interface provides a minimal interface for application wanting to 
manage their own memory on heterogeneous memory tiers. It also provides
an arena allocator that application developers can use to create `jemalloc` arenas
on different memory tiers and allocate to those tiers.

The `high` interface attempts to automatically manage the memory tiers for
the application. It provides an LLVM compiler pass (and compiler wrappers) to 
automatically transform applications to make the appropriate `high` interface
calls, as well as a runtime library which provides profiling for the application.
The profiling is currently meant to be used offline; that is, after enabling the
profiling for an application run, the results are printed out at the end of the run,
and that information must be fed into a second run to make use of it. An online
approach is planned.

To compile and install on a new system, run
```
./autogen.sh
./configure --prefix=[DIR]
make
make install
```

The only dependency that you will need for the low-level interface is `jemalloc`. The build system
will automatically look in `/usr` for this dependency, but if you want to provide a custom location,
you can pass the `--with-jemalloc` argument to `configure`.

For the high-level interface, you need an installation of LLVM. LLVM 4.0 and later have been tested, although
3.9 may possibly work. For the profiling, you will also need an installation of `libpfm`, which is a small
helper library for `perf` that is available on most distributions. These two dependencies are found automatically,
but again, the user can specify `--with-llvm=` and `--with-libpfm` to use a custom location.

[![Build Status](https://travis-ci.org/lanl/SICM.svg?branch=master)](https://travis-ci.org/lanl/SICM)
