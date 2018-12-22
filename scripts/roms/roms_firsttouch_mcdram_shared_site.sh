#!/bin/bash

export SICM_DIR="/lustre/atlas/scratch/molson5/gen010/SICM"
cd $SICM_DIR/examples/high/roms/run
source $SICM_DIR/scripts/all/firsttouch_all_shared_site.sh

firsttouch "1" "./roms < ocean_benchmark3.in"
