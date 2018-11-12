#!/bin/bash

DIR=`readlink -f ./deps`

cd examples/high/lulesh

# Do a firsttouch run with 12.5% MCDRAM
export SH_ARENA_LAYOUT="SHARED_SITE_ARENAS"
export SH_DEFAULT_NODE="1"
export OMP_NUM_THREADS=255
rm results/firsttouch_12.5%.txt
for iter in {1..5}; do
  echo 3 | sudo tee /proc/sys/vm/drop_caches
  # Reserve all but 12.5%
  cat results/pebs_128.txt | $DIR/bin/memreserve 1 256 ratio .125 &
  sleep 5

  ./lulesh2.0 -s 220 -i 20 -r 11 -b 0 -c 64 -p &>> results/firsttouch_12.5%.txt

  # Clean up
  pkill memreserve
  sleep 5
done
