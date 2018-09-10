#!/usr/bin/env bash
# This is a helper script that builds and installs SICM's deps,
# jemalloc and LLVM.

set -e

JEMALLOC=false
LLVM=false
MPI=false
DIR=`readlink -f ./build_deps`
INSTALLDIR=`readlink -f ./deps`

function usage() {
    echo "Usage: $0 [options]"
    echo "    Options:"
    echo "        --llvm         Download and build LLVM"
    echo "        --jemalloc     Download and build jemalloc"
    echo "        --mpi          Download build MPI"
    echo "        --build_dir    Directory to download and build dependencies (default: ${DIR})"
    echo "        --install_dir  Directory to install dependencies into       (default: ${INSTALLDIR})"
}

# Parse command line arguments
# https://stackoverflow.com/a/14203146
POSITIONAL=()
while [[ $# -gt 0 ]]
do
key="$1"

case $key in
    -h|--help)
        usage
        exit 0
        ;;
    --llvm)
        LLVM=true
        ;;
    --jemalloc)
        JEMALLOC=true
        ;;
    --mpi)
        MPI=true
        ;;
    --build_dir)
        shift
        DIR="$1"
        ;;
    --install_dir)
        shift
        INSTALLDIR="$1"
        ;;
    *)    # unknown option
        POSITIONAL+=("$1") # save it in an array for later
        ;;
esac

shift # past argument

done
set -- "${POSITIONAL[@]}" # restore positional parameters

mkdir -p $DIR
cd $DIR

if [[ "${LLVM}" = true ]]; then
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
  cmake -DCMAKE_INSTALL_PREFIX=${INSTALLDIR} ..
  make -j $(nproc --all)
  make install
fi

# Reset
cd $DIR

# if jemalloc is not found in the install directory
if [[ ( "${JEMALLOC}" = true ) && ! -d "${INSTALLDIR}/jemalloc" ]]; then
  # Download jemalloc
  if [[ ! -d jemalloc ]]; then
    git clone https://github.com/jemalloc/jemalloc.git
  fi

  # Compile and install jemalloc
  cd jemalloc
  ./autogen.sh
  mkdir build
  cd build
  ../configure --prefix=${INSTALLDIR}/jemalloc --with-jemalloc-prefix=je_
  make -j $(nproc --all)
  make -j $(nproc --all) -i install
fi

# Reset
cd $DIR

# if MPI is not found in the install directory
if [[ ( "${MPI}" = true ) && ! -d "${INSTALLDIR}/openmpi-3.1.1" ]]; then
  # if source directory doesn't exist, get it
  if [[ ! -d openmpi-3.1.1 ]]; then
    # if tarball doesn't exist, download it
    if [[ ! -f openmpi-3.1.1.tar.bz2 ]]; then
      wget https://download.open-mpi.org/release/open-mpi/v3.1/openmpi-3.1.1.tar.bz2
    fi

    tar xf openmpi-3.1.1.tar.bz2
  fi

  # Compile and install MPI
  cd openmpi-3.1.1
  mkdir -p build
  cd build
  ../configure --prefix=${INSTALLDIR}/openmpi-3.1.1 --without-verbs --without-fca --without-mxm --without-ucx --without-portals4 --without-psm --without-psm2 --without-libfabric --without-usnic --without-udreg --without-ugni --without-xpmem --without-alps --without-sge --without-tm --without-lsf --without-slurm --without-pvfs2 --without-plfs --without-cuda --disable-oshmem --enable-mpi-fortran --disable-oshmem-fortran --disable-libompitrace --disable-io-romio --disable-static &> /dev/null
  make -j $(nproc --all) &> /dev/null
  make -j $(nproc --all) install &> /dev/null
fi
