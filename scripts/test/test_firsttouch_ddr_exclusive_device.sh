#!/bin/bash

export SICM_DIR="/lustre/atlas/scratch/molson5/gen010/SICM"
cd $SICM_DIR/examples/high/test
source $SICM_DIR/scripts/all/firsttouch_all_exclusive_device.sh

firsttouch "0" "./stream.exe"
