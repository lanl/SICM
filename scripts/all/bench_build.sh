#!/bin/bash

export SICM_DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" >/dev/null && cd ../.. && pwd )"

# First argument is "fort" or "c", which linker we should use
function bench_build {
  if [ "$1" = "fort" ]; then
    export LD_LINKER="$SICM_DIR/deps/bin/flang -g -Wno-unused-command-line-argument"
  elif [ "$1" = "c" ]; then
    export LD_LINKER="$SICM_DIR/deps/bin/clang++ -g -Wno-unused-command-line-argument"
  else
    echo "No linker specified. Aborting."
    exit
  fi

  # Define the variables for the compiler wrappers
  export LD_COMPILER="$SICM_DIR/deps/bin/clang++ -Wno-unused-command-line-argument" # Compiles from .bc -> .o
  export CXX_COMPILER="$SICM_DIR/deps/bin/clang++ -g -Wno-unused-command-line-argument -I$SICM_DIR/deps/include"
  export FORT_COMPILER="$SICM_DIR/deps/bin/flang  -g -Mpreprocess -Wno-unused-command-line-argument -I$SICM_DIR/deps/include"
  export C_COMPILER="$SICM_DIR/deps/bin/clang -g -Wno-unused-command-line-argument -I$SICM_DIR/deps/include"
  export LLVMLINK="$SICM_DIR/deps/bin/llvm-link"
  export OPT="$SICM_DIR/deps/bin/opt"


  # Make sure the Lulesh Makefile finds our wrappers
  export COMPILER_WRAPPER="$SICM_DIR/deps/bin/compiler_wrapper.sh -g"
  export LD_WRAPPER="$SICM_DIR/deps/bin/ld_wrapper.sh -g"
  export PREPROCESS_WRAPPER="$DIR/bin/clang -E -x c -w -P"
}
