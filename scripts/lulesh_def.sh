#!/bin/bash

DIR=`readlink -f ./deps`

cd examples/high/lulesh_def

# Do a default run, use GNU time to get the peak RSS
export OMP_NUM_THREADS=255
rm ../lulesh/results/def.txt
for iter in {1..5}; do
  echo 3 | sudo tee /proc/sys/vm/drop_caches
  /usr/bin/time -v numactl --membind=0 ./lulesh2.0 -s 220 -i 20 -r 11 -b 0 -c 64 -p &>> ../lulesh/results/def.txt
done
