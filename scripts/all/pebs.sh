#!/bin/bash

export SICM_DIR="/lustre/atlas/scratch/molson5/gen010/SICM"
export PATH="$SICM_DIR/deps/bin:$PATH"
export SH_ARENA_LAYOUT="SHARED_SITE_ARENAS"
export SH_PROFILE_ALL="1"
export SH_PROFILE_ALL_RATE="0"
export SH_MAX_SAMPLE_PAGES="512"
export SH_PROFILE_RSS="1"
export SH_PROFILE_RSS_RATE="0"
export SH_DEFAULT_NODE="0"
export OMP_NUM_THREADS="256"

# Takes a PEBS frequency as an argument
# Second argument is the command to run
function pebs {
  export SH_SAMPLE_FREQ="${1}"

  # User output
  echo "Running experiment:"
  echo "  Experiment: PEBS Profiling"
  echo "  Sample Frequency: ${1}"
  echo "  Command: '${2}'"

  # Run 5 iters
  rm -rf results/pebs_${1}
  mkdir -p results/pebs_${1}
  for iter in {1..5}; do
    $SICM_DIR/deps/bin/memreserve 1 256 constant 4128116 release prefer # "Clear caches"
    eval "env time -v" "$2" &>> results/pebs_${1}/stdout.txt
  done
}
