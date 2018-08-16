#!/bin/bash

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
#make
make high
#make compass

cd examples/high/lulesh
#make clean
#make -j5
cd ../../..

# Turn off prefetching
sudo ../msr-tools/wrmsr -a 0x1A4 0xf
sudo ../msr-tools/rdmsr -a -x 0x1A4

# Turn on profiling and use 3 threads
export SH_PROFILING="yes"
export OMP_NUM_THREADS=3
export SH_ARENA_LAYOUT="SHARED_SITE_ARENAS"
cd examples/high/lulesh
time ./lulesh2.0
