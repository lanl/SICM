#!/bin/bash

export SICM_DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" >/dev/null && cd ../.. && pwd )"
source $SICM_DIR/scripts/all/bench_build.sh
bench_build "c" "-lmpi"

# Compile Lulesh
cd $SICM_DIR/examples/high/amg
make veryclean
make -j $(nproc --all)
cp test/amg .
