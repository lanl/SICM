#!/usr/bin/env bash
# This is a helper script that builds and installs SICM's deps,
# jemalloc, libpfm4, LLVM, and MPI.

set -e

JEMALLOC=false
LLVM=false
MPI=false
LIBPFM4=false
TIME=false
DIR=`readlink -f ./build_deps`
INSTALLDIR=`readlink -f ./deps`

function usage() {
    echo "Usage: $0 [options]"
    echo "    Options:"
    echo "        --llvm         Download and build LLVM"
    echo "        --jemalloc     Download and build jemalloc"
    echo "        --mpi          Download and build MPI"
    echo "        --libpfm4      Download and build libpfm4"
    echo "        --time         Download and build GNU time"
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
    --libpfm4)
        LIBPFM4=true
        ;;
    --time)
        TIME=true
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
  # Build LLVM
  git clone https://github.com/flang-compiler/llvm.git
  cd llvm
  git checkout release_60
  rm -rf build && mkdir build && cd build
  cmake -DCMAKE_INSTALL_PREFIX=${INSTALLDIR} ..
  make -j $(nproc --all)
  make install
  cd $DIR

  # Build flang driver
  git clone https://github.com/flang-compiler/flang-driver.git
  cd flang-driver
  git checkout release_60
  mkdir build && cd build
  cmake -DCMAKE_INSTALL_PREFIX=${INSTALLDIR} \
        -DLLVM_CONFIG=${INSTALLDIR}/bin/llvm-config ..
  make -j $(nproc --all)
  make install
  cd $DIR

  # Build OpenMP
  git clone https://github.com/llvm-mirror/openmp.git
  cd openmp/runtime
  git checkout release_60
  mkdir build && cd build
  cmake -DCMAKE_INSTALL_PREFIX=${INSTALLDIR} \
        -DLLVM_CONFIG=${INSTALLDIR}/bin/llvm-config  ../..
  make -j $(nproc --all)
  make install
  cd $DIR

  # Get flang itself, with SICM mods
  git clone https://github.com/benbenolson/flang.git

  # Compile libpgmath
  cd flang/runtime/libpgmath
  mkdir build && cd build
  cmake -DCMAKE_INSTALL_PREFIX=${INSTALLDIR} \
        -DLLVM_CONFIG=${INSTALLDIR}/bin/llvm-config \
        -DCMAKE_CXX_COMPILER=${INSTALLDIR}/bin/clang++ \
        -DCMAKE_C_COMPILER=${INSTALLDIR}/bin/clang \
        -DCMAKE_Fortran_COMPILER=${INSTALLDIR}/bin/flang ..
  make -j $(nproc --all)
  make install
  cd $DIR

  # Flang itself
  cd flang
  mkdir build && cd build
  cmake -DCMAKE_INSTALL_PREFIX=${INSTALLDIR} \
        -DLLVM_CONFIG=${INSTALLDIR}/bin/llvm-config \
        -DCMAKE_CXX_COMPILER=${INSTALLDIR}/bin/clang++ \
        -DCMAKE_C_COMPILER=${INSTALLDIR}/bin/clang \
        -DCMAKE_Fortran_COMPILER=${INSTALLDIR}/bin/flang ..
  make -j $(nproc --all) || true
  make
  make install
  cd $DIR

fi

# Reset
cd $DIR

# if jemalloc is not found in the install directory
if [[ ( "${JEMALLOC}" = true ) && ! -d "${INSTALLDIR}/jemalloc" ]]; then
  # Download jemalloc
  if [[ ! -d jemalloc ]]; then
    git clone https://github.com/benbenolson/jemalloc.git
  fi

  # Compile and install jemalloc
  cd jemalloc
  git checkout 5.1.0-mod
  ./autogen.sh
  mkdir build
  cd build
  ../configure --prefix=${INSTALLDIR} \
               --with-jemalloc-prefix=je_ \
               --disable-tcache
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
  ../configure --prefix=${INSTALLDIR} --with-pmi=/opt/cray/pe/pmi/5.0.10-1.0000.11050.0.0.ari/ --disable-dlopen --without-verbs --without-fca --without-mxm --without-ucx --without-portals4 --without-psm --without-psm2 --without-libfabric --without-usnic --without-udreg --without-ugni --without-xpmem --with-alps --without-sge --without-tm --without-lsf --without-slurm --without-pvfs2 --without-plfs --without-cuda --disable-oshmem --enable-mpi-fortran --disable-oshmem-fortran --disable-libompitrace --disable-io-romio --disable-static --enable-shared
  make -j $(nproc --all)
  make -j $(nproc --all) install
fi

cd $DIR

# if LIBPFM4
if [[ ( "${LIBPFM4}" = true ) ]]; then
  if [[ ! -d libpfm-4.10.1 ]]; then
    if [[ ! -f libpfm-4.10.1.tar.gz ]]; then
      wget https://sourceforge.net/projects/perfmon2/files/libpfm4/libpfm-4.10.1.tar.gz/download
      mv download libpfm-4.10.1.tar.gz
    fi
    tar xf libpfm-4.10.1.tar.gz
  fi

  cd libpfm-4.10.1
  make -j $(nproc --all)
  make PREFIX=${INSTALLDIR} -j $(nproc --all) install
fi

cd $DIR

if [[ ( "${TIME}" = true ) ]]; then
  if [[ ! -d time-1.9 ]]; then
    if [[ ! -f time-1.9.tar.gz ]]; then
      wget https://ftp.gnu.org/gnu/time/time-1.9.tar.gz
      tar xf time-1.9.tar.gz
    fi
    tar xf time-1.9.tar.gz
  fi

  cd time-1.9
  ./configure --prefix=${INSTALLDIR}
  make -j $(nproc --all)
  make -j $(nproc --all) install
fi
