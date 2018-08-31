#!/bin/bash
# This is a helper script that builds and installs SICM's deps,
# jemalloc and LLVM.

NOJEMALLOC=false
NOLLVM=false
for arg in "$@"; do 
  echo $arg;
  if [ "$arg" = "--no-llvm" ]; then
    echo "Not compiling LLVM."
    NOLLVM=true
  fi
  if [ "$arg" = "--no-jemalloc" ]; then
    echo "Not compiling jemalloc."
    NOJEMALLOC=true
  fi
done

DIR=`realpath ./build_deps`
INSTALLDIR=`realpath ./deps`
mkdir -p $DIR
cd $DIR

if [ "$NOLLVM" = false ]; then
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
  cmake -DCMAKE_INSTALL_PREFIX=$INSTALLDIR ..
  make -j $(nproc --all)
  make install
fi

# Reset
cd $DIR

if [ "$NOJEMALLOC" = false ]; then
  # Download and compile and install jemalloc
  git clone https://github.com/jemalloc/jemalloc.git
  cd jemalloc
  ./autogen.sh
  mkdir build
  cd build
  export JEPATH=$DIR
  ../configure --prefix=$INSTALLDIR --with-jemalloc-prefix=je_
  make -j $(nproc --all)
  make -j $(nproc --all) -i install
fi
