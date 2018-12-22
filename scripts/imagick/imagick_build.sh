#!/bin/bash

export SICM_DIR="/lustre/atlas/scratch/molson5/gen010/SICM"
source $SICM_DIR/scripts/all/bench_build.sh
bench_build "c"

cd $SICM_DIR/examples/high/imagick/src
make clean
make -j $(nproc --all)
cp imagick_s $SICM_DIR/examples/high/imagick/run/imagick
