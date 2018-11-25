#!/bin/bash

DIR=`readlink -f ./deps`

cd examples/high/lulesh

# Run the online approach
export SH_ARENA_LAYOUT="SHARED_SITE_ARENAS"
export SH_PROFILE_ALL="1"
export SH_PROFILE_ALL_RATE="0"
export SH_MAX_SAMPLE_PAGES="512"
export SH_SAMPLE_FREQ="128"
export SH_PROFILE_RSS="1"
export SH_PROFILE_RSS_RATE="0"
export SH_DEFAULT_NODE="0"
export OMP_NUM_THREADS="254"
export SH_ONLINE_PROFILING="1"
rm results/online_12.5%.txt
for iter in {1..5}; do
  # Reserve all but 12.5% of Lulesh's peak RSS
  cat results/pebs_128.txt | $DIR/bin/memreserve 1 256 ratio .125 &>> results/online_12.5%.txt &
  sleep 5
  #watch -t "(numastat -m) | tee -a results/online_12.5%_numastat.txt" &
  #sleep 2
  sudo /opt/drop_caches

  # Now run the online approach
  ./lulesh2.0 -s 220 -i 20 -r 11 -b 0 -c 64 -p &>> results/online_12.5%.txt
  #gdb ./lulesh2.0 ./core

  # Clean up
  #pkill numastat
  #pkill watch
  pkill memreserve
  sleep 5
done
