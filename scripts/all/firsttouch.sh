#!/bin/bash

export SICM_DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" >/dev/null && cd ../.. && pwd )"
export SH_ARENA_LAYOUT="SHARED_SITE_ARENAS"
export SH_DEFAULT_NODE="1"
export OMP_NUM_THREADS=256

# Takes a ratio for how much to reserve as an argument
# Second argument is the command to run
function firsttouch {
  # Checks
  if [ ! -r results/pebs_128.txt ]; then
    echo "ERROR: The file 'results/pebs_128.txt doesn't exist yet. Aborting."
    exit
  fi

  # User output
  echo "Running experiment:"
  echo "  Experiment: Firsttouch"
  echo "  Ratio: ${1}"
  echo "  Command: '${2}'"

  # Run 5 iters
  mkdir -p results
  rm results/firsttouch_${1}.txt
  for iter in {1..5}; do
    sudo /opt/drop_caches
    cat results/pebs_128.txt | $SICM_DIR/deps/bin/memreserve 1 256 ratio ${1} &
    sleep 5
    eval "/usr/bin/time -v" "$2" &>> results/firsttouch_${1}.txt
    pkill memreserve
    sleep 5
  done
}
