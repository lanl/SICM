#!/bin/bash

export SICM_DIR="/lustre/atlas/scratch/molson5/gen010/SICM"
source $SICM_DIR/scripts/all/pebs.sh
cd $SICM_DIR/examples/high/fotonik3d/run

pebs "128" "./fotonik3d"
