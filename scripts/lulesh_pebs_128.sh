#!/bin/bash

DIR=`readlink -f ./deps`

# Define the variables for the compiler wrappers
export LD_COMPILER="$DIR/bin/clang++"
export LD_LINKER="$DIR/bin/clang++"
export CXX_COMPILER="$DIR/bin/clang++"
export LLVMLINK="$DIR/bin/llvm-link"
export OPT="$DIR/bin/opt"

# Make sure the Lulesh Makefile finds our wrappers
export COMPILER_WRAPPER="$DIR/bin/compiler_wrapper.sh -g -DUSE_MPI=0"
export LD_WRAPPER="$DIR/bin/ld_wrapper.sh -g"

# Update SICM
make -j $(nproc --all)
make install

# Compile Lulesh
cd examples/high/lulesh
#make clean
#make -j $(nproc --all)

# Prefetching on
sudo -E env PATH="$PATH:$HOME/msr-tools" wrmsr -a 0x1A4 0x0

# Run PEBS to get profiling info
export SH_ARENA_LAYOUT="SHARED_SITE_ARENAS"
export SH_PROFILE_ALL="1"
export SH_MAX_SAMPLE_PAGES="512"
export SH_SAMPLE_FREQ="128"
export SH_PROFILE_RSS="1"
export SH_DEFAULT_NODE="0"
export OMP_NUM_THREADS=255
mkdir -p results
/usr/bin/time -v ./lulesh2.0 -s 220 -i 20 -r 11 -b 0 -c 64 -p &> results/pebs_128.txt
