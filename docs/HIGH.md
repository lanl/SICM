# High-Level Interface

The high-level interface is intended to be used with the compiler wrappers in the `bin/`
directory. Replace your C, C++, or FORTRAN compiler with calls to `compiler_wrapper.sh`,
and your linking step (even if done with a compiler call) with `ld_wrapper.sh`. There is
also an `ar_wrapper.sh` and `ranlib_wrapper.sh`.

These wrappers compile your application using LLVM and applies SICM's compiler pass
(`src/high/sicm_compass.cpp`). Once compiled, your application's allocation calls
will be replaced with calls into SICM's high-level interface. These functions are:
- `sh_alloc`
- `sh_realloc`
- `sh_calloc`
- `sh_aligned_alloc`
- `sh_posix_memalign`
- `sh_memalign`
- `sh_free`

Once you've got your application compiled, the compiler pass will output a file
called `contexts.txt` into the directory of the executable being linked. This is
simply a list of the allocation sites in your application. Each site lists an integer
ID, followed by the source code file and line number that the allocation site
comes from. This is followed by multiple lines of the call stack that lead up
to the allocation.

Now, when you run your newly-compiled executable, it will make calls into SICM's
high-level interface.

In general, there are four types of SICM high-level runs:
1. "Default" runs. These runs do not make any attempt to profile or move data in any way. They
   simply allocate memory using SICM's low-level arena allocator.
2. "Profiling" runs. These runs enable the profiling thread, and attempt to gather information
   about an application's behavior.
3. "Guided" runs. These runs come after a "profiling" run, and use the profiling information
   as guidance to steer allocations to the various memory tiers. They can be run with or
   without profiling enabled.
4. "Online" runs. These runs enable profiling, generate guidance, and directly apply it at runtime.
   They can read in a previously-run "profiling" run, but generally do not.
   
What the high-level interface does depends on the values
of a number of environment variables. These variables are parsed in `src/high/sicm_runtime_init.c`,
and for organizational purposes are split into two categories: "common" and "profiling."

## Common Options

Common options are those that apply to all SICM high-level runs.

| Environment Variable | Description |
|---------------|-------------|
| `SH_UPPER_NODE` | This is the NUMA node integer ID which SICM treats as the "upper", or more performant, tier of memory. |
| `SH_LOWER_NODE` | This is the NUMA node integer ID which SICM treats as the "lower" tier of memory. |
| `SH_DEFAULT_NODE` | This is a NUMA node integer ID which you'd like allocations to default to. |
| `SH_MAX_SITES_PER_ARENA` | This is the maximum number of allocations sites that SICM will allow into each arena. |
| `SH_MAX_THREADS` | This is the maximum number of threads that SICM will allow allocations from. |
| `SH_MAX_ARENAS` | This is the maximum number of arenas. Defaults to 4096. Usually, this is limited by your arena allocator (in this case, `jemalloc`). |
| `SH_ARENA_LAYOUT` | This is how SICM associates allocation sites and arenas. See section "Arena Layout" for more information. |
