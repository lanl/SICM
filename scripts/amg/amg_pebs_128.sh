#!/bin/bash

export SICM_DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" >/dev/null && cd ../.. && pwd )"
source $SICM_DIR/scripts/all/pebs.sh
cd $SICM_DIR/examples/high/amg

pebs "128" "./amg -problem 2 -n 120 120 120"
