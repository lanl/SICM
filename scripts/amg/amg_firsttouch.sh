#!/bin/bash

export SICM_DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" >/dev/null && cd ../.. && pwd )"
source $SICM_DIR/scripts/all/firsttouch.sh
cd $SICM_DIR/examples/high/amg

firsttouch "0" "./amg -problem 2 -n 120 120 120"
