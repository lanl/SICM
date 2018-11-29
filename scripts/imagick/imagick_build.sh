#!/bin/bash

export SICM_DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" >/dev/null && cd ../.. && pwd )"
source $SICM_DIR/scripts/all/bench_build.sh
bench_build "c"

cd $SICM_DIR/examples/high/imagick/src
make clean
make -j $(nproc --all)
cp imagick_s $SICM_DIR/examples/high/imagick/run/imagick
