#!/bin/bash

export SICM_DIR="/lustre/atlas/scratch/molson5/gen010/SICM"

# First argument is "fort" or "c", which linker we should use
# Second argument is a list of linker flags to add, which are just appended
# Third argument is a list of compiler flags to add, which are just appended
function bench_build {
  if [ "$1" = "fort" ]; then
    export LD_LINKER="$SICM_DIR/deps/bin/flang $2 -g -Wno-unused-command-line-argument -L$SICM_DIR/deps/lib -Wl,-rpath,$SICM_DIR/deps/lib"
  elif [ "$1" = "c" ]; then
    export LD_LINKER="$SICM_DIR/deps/bin/clang++ $2 -g -Wno-unused-command-line-argument -L$SICM_DIR/deps/lib -Wl,-rpath,$SICM_DIR/deps/lib"
  else
    echo "No linker specified. Aborting."
    exit
  fi

  # Define the variables for the compiler wrappers
  export LD_COMPILER="$SICM_DIR/deps/bin/clang++ -Wno-unused-command-line-argument -Ofast -march=knl" # Compiles from .bc -> .o
  export CXX_COMPILER="$SICM_DIR/deps/bin/clang++ $3 -g -Wno-unused-command-line-argument -Ofast -I$SICM_DIR/deps/include -march=knl"
  export FORT_COMPILER="$SICM_DIR/deps/bin/flang $3 -g -Mpreprocess -Wno-unused-command-line-argument -Ofast -I$SICM_DIR/deps/include -march=knl"
  export C_COMPILER="$SICM_DIR/deps/bin/clang -g $3 -Wno-unused-command-line-argument -Ofast -I$SICM_DIR/deps/include -march=knl"
  export LLVMLINK="$SICM_DIR/deps/bin/llvm-link"
  export OPT="$SICM_DIR/deps/bin/opt"


  # Make sure the Lulesh Makefile finds our wrappers
  export COMPILER_WRAPPER="$SICM_DIR/deps/bin/compiler_wrapper.sh -g"
  export LD_WRAPPER="$SICM_DIR/deps/bin/ld_wrapper.sh -g"
  export PREPROCESS_WRAPPER="$SICM_DIR/deps/bin/clang -E -x c -w -P"
}

# First argument is "fort" or "c", which linker we should use.
# For the default build, this will also be the compiler that we use. This should be fixed later to allow
# for multiple compilers.
# Second argument is a list of linker flags to add, which are just appended
function def_bench_build {
  if [ "$1" = "fort" ]; then
    export LD_WRAPPER="$SICM_DIR/deps/bin/flang $2 -g -Wno-unused-command-line-argument -L$SICM_DIR/deps/lib -Wl,-rpath,$SICM_DIR/deps/lib "
    export COMPILER_WRAPPER="$SICM_DIR/deps/bin/flang $3 -g -Wno-unused-command-line-argument -I$SICM_DIR/deps/include"
  elif [ "$1" = "c" ]; then
    export LD_WRAPPER="$SICM_DIR/deps/bin/clang++ $2 -g -Wno-unused-command-line-argument -L$SICM_DIR/deps/lib -Wl,-rpath,$SICM_DIR/deps/lib"
    export COMPILER_WRAPPER="$SICM_DIR/deps/bin/clang $3 -g -Wno-unused-command-line-argument -I$SICM_DIR/deps/include"
  else
    echo "No linker specified. Aborting."
    exit
  fi

  export PREPROCESS_WRAPPER="$SICM_DIR/bin/clang -E -x c -w -P"
}
