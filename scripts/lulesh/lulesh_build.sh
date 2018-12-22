#!/bin/bash

export SICM_DIR="/lustre/atlas/scratch/molson5/gen010/SICM"
source $SICM_DIR/scripts/all/bench_build.sh
bench_build c

# Compile Lulesh
cd $SICM_DIR/examples/high/lulesh
make clean
make -j $(nproc --all)
