#!/bin/bash

export SICM_DIR="/lustre/atlas/scratch/molson5/gen010/SICM"
cd $SICM_DIR/examples/high/fotonik3d/run
source $SICM_DIR/scripts/all/offline_pebs.sh

pebs "128" "50" "hotset" "./fotonik3d"
