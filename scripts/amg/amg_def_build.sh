#!/bin/bash

export SICM_DIR="/lustre/atlas/scratch/molson5/gen010/SICM"
source $SICM_DIR/scripts/all/bench_build.sh
def_bench_build "c" "-lmpi"

# Compile Lulesh
cd $SICM_DIR/examples/high/amg
#make veryclean
make -j $(nproc --all)
cp test/amg .
