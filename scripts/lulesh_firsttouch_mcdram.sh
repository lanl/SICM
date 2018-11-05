#!/bin/bash

DIR=`readlink -f ./deps`

cd examples/high/lulesh

# Do an all-DDR run with SICM
export SH_ARENA_LAYOUT="SHARED_SITE_ARENAS"
export SH_DEFAULT_NODE="1"
export OMP_NUM_THREADS=255
rm results/firsttouch_mcdram.txt
for iter in {1..5}; do
  echo 3 | sudo tee /proc/sys/vm/drop_caches
  ./lulesh2.0 -s 220 -i 20 -r 11 -b 0 -c 64 -p &>> results/firsttouch_mcdram.txt
done
