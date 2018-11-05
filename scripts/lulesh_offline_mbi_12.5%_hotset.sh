#!/bin/bash

DIR=`readlink -f ./deps`

cd examples/high/lulesh

# Do an MBI-guided run
export SH_ARENA_LAYOUT="SHARED_SITE_ARENAS"
export SH_DEFAULT_NODE="0"
export SH_GUIDANCE_FILE="results/guidance/mbi_12.5%_hotset.txt"
export OMP_NUM_THREADS=255
rm results/offline_mbi_12.5%_hotset.txt

mkdir -p results/guidance

for iter in {1..5}; do
  # Generate a knapsack, 12.5%
  echo 3 | sudo tee /proc/sys/vm/drop_caches
  grep -Rh "" results/mbi results/pebs_128.txt | $DIR/bin/hotset mbi hotset ratio 0.125 1 > results/guidance/mbi_12.5%_hotset.txt

  # Reserve all but 12.5%
  cat results/pebs_128.txt | $DIR/bin/memreserve 1 256 ratio .125 &
  sleep 5

  ./lulesh2.0 -s 220 -i 20 -r 11 -b 0 -c 64 -p &>> results/offline_mbi_12.5%_hotset.txt

  # Clean up
  sudo pkill memreserve
  sleep 5
done
