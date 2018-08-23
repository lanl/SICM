#!/bin/bash

# Get the MSR tools
export PATH="$PATH:$HOME/msr-tools"

# Define the variables for the compiler wrappers
export LD_COMPILER="clang++-4.0"
export LD_LINKER="clang++-4.0"
export CXX_COMPILER="clang++-4.0"
export LLVMLINK="llvm-link-4.0"
export OPT="opt-4.0"

# Make sure the Lulesh Makefile finds our wrappers
export COMPILER_WRAPPER="../../../bin/compiler_wrapper.sh -g -DUSE_MPI=0"
export LD_WRAPPER="../../../bin/ld_wrapper.sh -g"
#export C_WRAPPER="clang-4.0 -g"
#export LD_WRAPPER="clang-4.0 -g"

# Compile SICM
make clean
make
make high
make compass

# Compile Lulesh
cd examples/high/lulesh
make clean
make -j5

# Now we're going to test profiling overhead
export SH_PROFILING="yes"
export SH_ARENA_LAYOUT="SHARED_SITE_ARENAS"
export SH_MAX_SAMPLE_PAGES="512"
export SH_SAMPLE_FREQ="512"

# Prefetching off
sudo -E env PATH="$PATH:$HOME/msr-tools" wrmsr -a 0x1A4 0xf

# 3 threads
export OMP_NUM_THREADS=3
time sudo -E ./lulesh2.0 -s 45
