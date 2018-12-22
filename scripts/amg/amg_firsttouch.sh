#!/bin/bash

export SICM_DIR="/lustre/atlas/scratch/molson5/gen010/SICM"
source $SICM_DIR/scripts/all/firsttouch.sh
cd $SICM_DIR/examples/high/amg

firsttouch "0" "./amg -problem 2 -n 120 120 120"
