#!/bin/bash

DIR=`readlink -f ./deps`

cd examples/high/lulesh

mkdir -p results/guidance

# Do a PEBS-guided run
export SH_GUIDANCE_FILE="results/guidance/pebs_12.5%_knapsack.txt"
export SH_ARENA_LAYOUT="SHARED_SITE_ARENAS"
export SH_DEFAULT_NODE="0"
export OMP_NUM_THREADS=255
rm results/offline_pebs_12.5%_knapsack.txt
for iter in {1..5}; do

  # Generate a knapsack, 12.5%
  echo 3 | sudo tee /proc/sys/vm/drop_caches
  grep -Rh "" results/pebs_128.txt | $DIR/bin/hotset pebs knapsack ratio 0.125 1 > results/guidance/pebs_12.5%_knapsack.txt

  # Reserve all but 12.5%
  cat results/pebs_128.txt | $DIR/bin/memreserve 1 256 ratio .125 &
  sleep 5

  ./lulesh2.0 -s 220 -i 20 -r 11 -b 0 -c 64 -p &>> results/offline_pebs_12.5%_knapsack.txt

  # Clean up
  pkill memreserve
  sleep 5
done
