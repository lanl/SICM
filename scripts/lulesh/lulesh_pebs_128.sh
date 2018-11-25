#!/bin/bash

DIR=`readlink -f ./deps`

cd examples/high/lulesh

# Prefetching on
#sudo -E env PATH="$PATH:$HOME/msr-tools" wrmsr -a 0x1A4 0x0

# Run PEBS to get profiling info
export SH_ARENA_LAYOUT="SHARED_SITE_ARENAS"
export SH_PROFILE_ALL="1"
export SH_PROFILE_ALL_RATE="0"
export SH_MAX_SAMPLE_PAGES="512"
export SH_SAMPLE_FREQ="128"
export SH_PROFILE_RSS="1"
export SH_PROFILE_RSS_RATE="0"
export SH_DEFAULT_NODE="0"
export OMP_NUM_THREADS=255
mkdir -p results
./lulesh2.0 -s 220 -i 20 -r 11 -b 0 -c 64 -p &> results/pebs_128.txt
