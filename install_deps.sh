#!/bin/bash
# This is a helper script that accepts a directory as an argument,
# and installs jemalloc and llvm into that directory.

if [ $# -eq 0 ]; then
	echo "No arguments supplied"
fi

DIR=$1
cd $DIR

# Download the tarball for LLVM
wget http://releases.llvm.org/4.0.1/llvm-4.0.1.src.tar.xz
tar xf llvm-4.0.1.src.tar.xz
rm llvm-4.0.1.src.tar.xz
mv llvm-4.0.1.src llvm
cd llvm

# Clang
cd tools
wget http://releases.llvm.org/4.0.1/cfe-4.0.1.src.tar.xz
tar xf cfe-4.0.1.src.tar.xz
rm cfe-4.0.1.src.tar.xz
mv cfe-4.0.1.src clang
cd ..

# OpenMP
cd projects
wget http://releases.llvm.org/4.0.1/openmp-4.0.1.src.tar.xz
tar xf openmp-4.0.1.src.tar.xz
rm openmp-4.0.1.src.tar.xz
mv openmp-4.0.1.src openmp
cd ..

# Compile and install LLVM
rm -rf build
mkdir build
cd build
cmake -DCMAKE_INSTALL_PREFIX=$DIR ..
make -j $(nproc --all)
make install

# Reset
cd $DIR

# Download and compile and install jemalloc
git clone https://github.com/jemalloc/jemalloc.git
cd jemalloc
./autogen.sh
mkdir build
cd build
export JEPATH=$DIR
../configure --prefix=$DIR --with-jemalloc-prefix=je_
make -j $(nproc --all)
make -j $(nproc --all) -i install
