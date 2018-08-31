#!/bin/bash
# Compiles and runs Lulesh, using the SICM installation in the directory
# that you pass as an argument.
set -e

DIR=`realpath ./deps`

# Define the variables for the compiler wrappers
export LD_COMPILER="clang++-3.9"
export LD_LINKER="clang++-3.9"
export CXX_COMPILER="clang++-3.9"
export LLVMLINK="llvm-link-3.9"
export OPT="opt-3.9"

# Make sure the Lulesh Makefile finds our wrappers
export COMPILER_WRAPPER="$DIR/bin/compiler_wrapper.sh -g -DUSE_MPI=0"
export LD_WRAPPER="$DIR/bin/ld_wrapper.sh -g"
#export C_WRAPPER="clang-4.0 -g"
#export LD_WRAPPER="clang-4.0 -g"

# Compile SICM
make clean || true
make distclean || true
make uninstall || true
./autogen.sh
./configure CPPFLAGS="-I/usr/include/llvm-3.9 -I/usr/include/llvm-c-3.9" --prefix=$DIR --with-jemalloc=$DIR
make -j5
make install

# Compile Lulesh
cd examples/high/lulesh
make clean
make -j5

# Now we're going to test profiling overhead
#export SH_PROFILING="yes"
export SH_ARENA_LAYOUT="SHARED_SITE_ARENAS"
export SH_MAX_SAMPLE_PAGES="512"
export SH_SAMPLE_FREQ="512"

# Prefetching off
#sudo -E env PATH="$PATH:$HOME/msr-tools" wrmsr -a 0x1A4 0xf
sudo -E env PATH="$PATH:$HOME/msr-tools" wrmsr -a 0x1A4 0x0

# 3 threads
export OMP_NUM_THREADS=3
valgrind --leak-check=full --show-leak-kinds=all ./lulesh2.0 -s 5
