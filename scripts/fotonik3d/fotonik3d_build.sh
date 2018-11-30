#!/bin/bash

export SICM_DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" >/dev/null && cd ../.. && pwd )"
source $SICM_DIR/scripts/all/bench_build.sh
bench_build "fort"

# Compile Lulesh
cd $SICM_DIR/examples/high/fotonik3d/src
make clean
make -j $(nproc --all)
cp fotonik3d_s $SICM_DIR/examples/high/fotonik3d/run/fotonik3d
