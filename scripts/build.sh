#!/bin/bash

export SICM_DIR="/lustre/atlas/scratch/molson5/gen010/SICM"
#export CC="gcc"
#export CXX="g++ -fPIC -shared"
#export LD="/opt/gcc/6.2.0/bin/g++"
#export CCLD="/opt/gcc/6.2.0/bin/g++ --disable-static --enable-shared"
#export CXXLD="/opt/gcc/6.2.0/bin/g++ --disable-static --enable-shared"
export CRAYPE_LINK_TYPE=dynamic

# Compile SICM
make uninstall || true
#make distclean || true
#./autogen.sh
#./configure --prefix=$SICM_DIR/deps --with-jemalloc=$SICM_DIR/deps --with-llvm=$($SICM_DIR/deps/bin/llvm-config --prefix) --with-libpfm=$SICM_DIR/deps
make clean
make -j5
make install
