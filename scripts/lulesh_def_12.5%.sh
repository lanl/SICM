#!/bin/bash

DIR=`readlink -f ./deps`

cd examples/high/lulesh_def

# Do a default run
export OMP_NUM_THREADS=255
rm ../lulesh/results/def_12.5%.txt
for iter in {1..5}; do
  echo 3 | sudo tee /proc/sys/vm/drop_caches
  # 12.5% firsttouch
  cat ../lulesh/results/def.txt | ../../../deps/bin/memreserve 1 256 ratio .125 &
  sleep 5
  numactl --membind=1 ./lulesh2.0 -s 220 -i 20 -r 11 -b 0 -c 64 -p &>> ../lulesh/results/def_12.5%.txt
  # Clean up
  pkill memreserve
  sleep 5
done
