#!/bin/bash

DIR=`readlink -f ./deps`

# Define the variables for the compiler wrappers
export LD_COMPILER="$DIR/bin/clang++ -g -Wno-unused-command-line-argument"
export LD_LINKER="$DIR/bin/flang -g -Wno-unused-command-line-argument"
export CXX_COMPILER="$DIR/bin/clang++ -g"
export FORT_COMPILER="$DIR/bin/flang -Wno-unused-command-line-argument -g"
export LLVMLINK="$DIR/bin/llvm-link"
export OPT="$DIR/bin/opt"

# Make sure the roms Makefile finds our wrappers
export COMPILER_WRAPPER="$DIR/bin/compiler_wrapper.sh"
export LD_WRAPPER="$DIR/bin/ld_wrapper.sh"
export PREPROCESS_WRAPPER="$DIR/bin/clang -E -x c"
#export COMPILER_WRAPPER="$DIR/bin/flang -Mpreprocess -fopenmp -g"
#export LD_WRAPPER="$DIR/bin/flang -g -L${DIR}/lib -lhigh -Wl,-rpath,${DIR}/lib"

# Compile roms
cd examples/high/roms/src
make clean
make -j $(nproc --all)
cp sroms ../run/roms
