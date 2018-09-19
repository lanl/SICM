#!/bin/bash

DIR=`readlink -f ./deps`

# Compile SICM
make uninstall || true
make distclean || true
./autogen.sh
./configure --prefix=$DIR --with-jemalloc=$DIR/jemalloc --with-llvm=$(llvm-config-4.0 --prefix)
make -j5
make install
