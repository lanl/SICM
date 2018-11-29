#!/bin/bash

export SICM_DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" >/dev/null && cd .. && pwd )"

# Compile SICM
make uninstall || true
make distclean || true
./autogen.sh
./configure --prefix=$SICM_DIR/deps --with-jemalloc=$SICM_DIR/deps --with-llvm=$($SICM_DIR/deps/bin/llvm-config --prefix) --with-libpfm=$SICM_DIR/deps
make -j5
make install
