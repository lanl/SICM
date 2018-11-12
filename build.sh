#!/bin/bash

DIR=`readlink -f ./deps`

# Compile SICM
make uninstall || true
make distclean || true
./autogen.sh
./configure --prefix=$DIR --with-jemalloc=$DIR/jemalloc --with-libpfm=$DIR --with-llvm=$($DIR/bin/llvm-config --prefix)
make -j5
make install
