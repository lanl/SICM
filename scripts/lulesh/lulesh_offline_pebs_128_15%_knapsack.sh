#!/bin/bash

export SICM_DIR="/lustre/atlas/scratch/molson5/gen010/SICM"
cd $SICM_DIR/examples/high/lulesh
source $SICM_DIR/scripts/all/offline_pebs.sh

pebs "128" "15" "knapsack" "./lulesh2.0 -s 220 -i 20 -r 11 -b 0 -c 64 -p"
