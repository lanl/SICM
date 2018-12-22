#!/bin/bash

export SICM_DIR="/lustre/atlas/scratch/molson5/gen010/SICM"
source $SICM_DIR/scripts/all/pebs.sh
cd $SICM_DIR/examples/high/amg

pebs "128" "./amg -problem 2 -n 120 120 120"
