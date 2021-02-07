# SICM
Simplified Interface to Complex Memory

[![Build Status](https://travis-ci.org/lanl/SICM.svg?branch=master)](https://travis-ci.org/lanl/SICM)

## Introduction
This is SICM, which is a tool to manually and automatically manage memory
in heterogeneous memory systems. It provides two interfaces: a `low` interface
and a `high` one.

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

## Further Documentation

Notes on installation, usage, programming practices, and examples are provided
in the `docs` directory. Please consult these documents for more specific information.
