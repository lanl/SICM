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

# Compile Lulesh
cd examples/high/lulesh
make clean
make -j $(nproc --all)
