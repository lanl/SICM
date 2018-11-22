#!/bin/bash

DIR=`readlink -f ./deps/`
PWD=`readlink -f ./examples/high/starlord`
#export PATH="$PATH:$DIR/bin"

# Define the variables for the compiler wrappers
export LD_COMPILER="$DIR/bin/clang++"
export LD_LINKER="$DIR/bin/clang++"
export CXX_COMPILER="$DIR/bin/clang++"
export C_COMPILER="$DIR/bin/clang"
export FORT_COMPILER="$DIR/bin/flang"
export LLVMLINK="$DIR/bin/llvm-link"
export OPT="$DIR/bin/opt"

export LD_WRAPPER="$DIR/bin/ld_wrapper.sh"
export COMPILER_WRAPPER="$DIR/bin/compiler_wrapper.sh"
export AMREX_HOME="$PWD/amrex"
export MICROPHYSICS_HOME="$PWD/Microphysics"

cd $PWD/Exec
make clean
make -j $(nproc --all) &>> log.txt
