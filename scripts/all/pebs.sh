#!/bin/bash

export SICM_DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" >/dev/null && cd ../.. && pwd )"
export SH_ARENA_LAYOUT="SHARED_SITE_ARENAS"
export SH_PROFILE_ALL="1"
export SH_PROFILE_ALL_RATE="0"
export SH_MAX_SAMPLE_PAGES="512"
export SH_PROFILE_RSS="1"
export SH_PROFILE_RSS_RATE="0"
export SH_DEFAULT_NODE="0"
export OMP_NUM_THREADS=254

# Takes a PEBS frequency as an argument
# Second argument is the command to run
function pebs {

  export SH_SAMPLE_FREQ="${1}"

  # User output
  echo "Running experiment:"
  echo "  Experiment: PEBS Profiling"
  echo "  Ratio: ${1}"
  echo "  Command: '${2}'"

  # Run 5 iters
  mkdir -p results
  rm results/pebs_${1}.txt
  for iter in {1..5}; do
    sudo /opt/drop_caches
    eval "/usr/bin/time -v" "$2" &>> results/pebs_${1}.txt
  done
}
