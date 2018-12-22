#!/bin/bash

export SICM_DIR="/lustre/atlas/scratch/molson5/gen010/SICM"
source $SICM_DIR/scripts/all/bench_build.sh
bench_build "c" "-L/opt/cray/pe/mpt/7.5.0/gni/mpich-gnu/4.9/lib/ -lmpich" "-I/opt/cray/pe/mpt/7.5.0/gni/mpich-gnu/4.9/include"

# Compile Lulesh
cd $SICM_DIR/examples/high/amg
make veryclean
make -j $(nproc --all)
cp test/amg .
