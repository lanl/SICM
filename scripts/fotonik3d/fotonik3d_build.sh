#!/bin/bash

export SICM_DIR="/lustre/atlas/scratch/molson5/gen010/SICM"
source $SICM_DIR/scripts/all/bench_build.sh
bench_build "fort"

# Compile Lulesh
cd $SICM_DIR/examples/high/fotonik3d/src
make clean
make -j $(nproc --all)
cp fotonik3d_s $SICM_DIR/examples/high/fotonik3d/run/fotonik3d
