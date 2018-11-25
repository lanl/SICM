#!/bin/bash

DIR=`readlink -f ./deps`

# Define the variables for the compiler wrappers
export LD_COMPILER="$DIR/bin/clang++"
export LD_LINKER="$DIR/bin/flang"
export CXX_COMPILER="$DIR/bin/clang++"
export FORT_COMPILER="$DIR/bin/flang -Mpreprocess -I${MPI_INCLUDE} -Wno-unused-command-line-argument"
export LLVMLINK="$DIR/bin/llvm-link"
export OPT="$DIR/bin/opt"

# Make sure the Lulesh Makefile finds our wrappers
export COMPILER_WRAPPER="$DIR/bin/compiler_wrapper.sh"
export LD_WRAPPER="$DIR/bin/ld_wrapper.sh"

# Compile Lulesh
cd examples/high/fotonik3d/src
make clean
make -j $(nproc --all)
