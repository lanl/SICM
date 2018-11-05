#!/bin/bash

DIR=`readlink -f ./deps`

# Make sure the Lulesh Makefile finds our wrappers
export COMPILER_WRAPPER="$DIR/bin/clang++ -g -DUSE_MPI=0"
export LD_WRAPPER="$DIR/bin/clang++ -g"

# Compile Lulesh
cd examples/high/lulesh_def
make clean
make -j $(nproc --all)
