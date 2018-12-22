#!/bin/bash

export SICM_DIR="/lustre/atlas/scratch/molson5/gen010/SICM"
export PATH="$SICM_DIR/deps/bin:$PATH"
export SH_ARENA_LAYOUT="SHARED_SITE_ARENAS"
export OMP_NUM_THREADS=256

# First argument is node to firsttouch onto
# Second argument is the command to run
function firsttouch {
  # User output
  echo "Running experiment:"
  echo "  Experiment: Firsttouch Shared Site, 100%"
  echo "  Node: ${1}"
  echo "  Command: '${2}'"

  # Default to the given node
  export SH_DEFAULT_NODE="${1}"

  # Run 5 iters
  rm -rf results/firsttouch_100_node${1}_shared_site/
  mkdir -p results/firsttouch_100_node${1}_shared_site/
  for iter in {1..5}; do
    $SICM_DIR/deps/bin/memreserve 1 256 constant 4128116 release prefer # "Clear caches"
    eval "env time -v numactl --preferred=${1}" "$2" &>> results/firsttouch_100_node${1}_shared_site/stdout.txt
  done
}
