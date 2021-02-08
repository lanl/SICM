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
and for organizational purposes are split into three categories: "common", "guided", and "profiling."

## Common Options

Common options are those that apply to all SICM high-level runs. They are parsed by the function `set_common_options` in
`src/high/sicm_runtime_init.c`.

| Environment Variable | Description |
|---------------|-------------|
| `SH_UPPER_NODE` | This is the NUMA node integer ID which SICM treats as the "upper", or more performant, tier of memory. |
| `SH_LOWER_NODE` | This is the NUMA node integer ID which SICM treats as the "lower" tier of memory. |
| `SH_DEFAULT_NODE` | This is a NUMA node integer ID which you'd like allocations to default to. |
| `SH_MAX_SITES_PER_ARENA` | This is the maximum number of allocations sites that SICM will allow into each arena. |
| `SH_MAX_THREADS` | This is the maximum number of threads that SICM will allow allocations from. For an OpenMP application, for example, I'll set this to `OMP_NUM_THREADS + 1`. |
| `SH_MAX_ARENAS` | This is the maximum number of arenas. Defaults to 4096. Usually, this is limited by your arena allocator (in this case, `jemalloc`). |
| `SH_MAX_SITES` | This is the maximum number of allocation sites. You can find this experimentally. |
| `SH_LOG_FILE` | This is a file which SICM outputs its configuration to, on initialization. |
| `SH_ARENA_LAYOUT` | This is how SICM allocates data from allocation sites into the various arenas. See section "Arena Layout" for more information. |

## Guided Options

Guided options are those that apply to SICM high-level runs which use offline profiling to guide their placement decisions. They are parsed by
the function `set_guided_options` in `src/high/sicm_runtime_init.c`.

| Environment Variable | Description |
|---------------|-------------|
| `SH_GUIDANCE_FILE` | This is a path to the guidance file. The guidance file is described later in this section. |

The guidance file is simply a text file in a format which tells SICM which tier to place each allocation site on. The first
line is `===== GUIDANCE =====` and the last line is `===== END GUIDANCE =====`. Each line in between includes
an allocation site ID (the very same that are listed in `contexts.txt`), followed by a space, followed by a NUMA node number.

If you know the intricacies of your application, you can manually construct a guidance file if you so choose. The example below should
be sufficient to tell you how to do this.

Under normal conditions, however, this file should be generated. SICM provides a small program, in the form of `src/high/sicm_hotset.c`,
to help do this with a variety of packing algorithms. This program takes as input offline profiling data, and outputs a guidance
file which packs allocation sites into a user-defined capacity. When you installed SICM, it will be installed as an executable called
`sicm_hotset`.

## Profiling Options

Profiling options are those that control how SICM profiles the application. This includes some kind of value capacity (generally PEBS
events on Intel processors), coupled with some method of gathering the capacity of each allocation site. This profiling can either
be output to file, where it is parsed and converted into memory tier guidance, or it can be used as an online profile to guide
the current runtime's placement decisions.

