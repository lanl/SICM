#!/bin/bash

export SICM_DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" >/dev/null && cd ../.. && pwd )"
source $SICM_DIR/scripts/all/pebs.sh
cd $SICM_DIR/examples/high/fotonik3d/run

pebs "128" "./fotonik3d"
