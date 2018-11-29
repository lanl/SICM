#!/bin/bash

DIR=`readlink -f ./deps`

# Define the variables for the compiler wrappers
export LD_COMPILER="$DIR/bin/clang++ -g -Wno-unused-command-line-argument"
export LD_LINKER="$DIR/bin/clang++ -g -Wno-unused-command-line-argument"
export C_COMPILER="$DIR/bin/clang -g -Wno-unused-command-line-argument"
export CXX_COMPILER="$DIR/bin/clang++ -g -Wno-unused-command-line-argument"
export FORT_COMPILER="$DIR/bin/flang -Mpreprocess -Wno-unused-command-line-argument -g"
export LLVMLINK="$DIR/bin/llvm-link"
export OPT="$DIR/bin/opt"

# Make sure the QMCPACK Makefile finds our wrappers
export COMPILER_WRAPPER="$DIR/bin/compiler_wrapper.sh"
export LD_WRAPPER="$DIR/bin/ld_wrapper.sh"
export PREPROCESS_WRAPPER="$DIR/bin/clang -E -x c -w -P"
#export COMPILER_WRAPPER="$DIR/bin/flang -Mpreprocess -fopenmp -g"
#export LD_WRAPPER="$DIR/bin/flang -g -L${DIR}/lib -lhigh -Wl,-rpath,${DIR}/lib"

# Compile QMCPACK
cd examples/high/qmcpack
rm -rf build
mkdir build
cd build
cmake -DCMAKE_CXX_COMPILER=$(COMPILER_WRAPPER) \
      -DCMAKE_C_COMPILER=$(COMPILER_WRAPPER) \
      ..
