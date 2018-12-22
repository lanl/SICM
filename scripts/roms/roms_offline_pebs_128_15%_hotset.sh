#!/bin/bash

export SICM_DIR="/lustre/atlas/scratch/molson5/gen010/SICM"
cd $SICM_DIR/examples/high/roms/run
source $SICM_DIR/scripts/all/offline_pebs.sh

pebs "128" "15" "hotset" "./roms < ocean_benchmark3.in"
