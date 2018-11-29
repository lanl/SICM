#!/bin/bash

DIR=`readlink -f ./deps`

cd examples/high/roms/run

# Do an all-DDR run with SICM
export SH_ARENA_LAYOUT="SHARED_SITE_ARENAS"
export SH_DEFAULT_NODE="0"
export OMP_NUM_THREADS="256"
mkdir -p results
rm results/firsttouch.txt
for iter in {1..5}; do
  sudo /opt/drop_caches
  ./roms < short_ocean_benchmark3.in &>> results/firsttouch.txt
done
