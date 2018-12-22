#!/bin/bash

export SICM_DIR="/lustre/atlas/scratch/molson5/gen010/SICM"
export PATH="$SICM_DIR/deps/bin:$PATH"
export OMP_NUM_THREADS=256
export SH_ARENA_LAYOUT="EXCLUSIVE_DEVICE_ARENAS"

# First arg is directory to write to
function background {
  rm results/$1/numastat.txt
  while true; do
    echo "=======================================" &>> results/$1/numastat.txt
    numastat -m &>> results/$1/numastat.txt
    sleep 2
  done
}


# Takes a percentage
# Second argument is the command to run
function firsttouch {
  # Checks
  if [ ! -r results/pebs_128/stdout.txt ]; then
    echo "ERROR: The file 'results/pebs_128/stdout.txt doesn't exist yet. Aborting."
    exit
  fi

  RATIO=$(echo "${1}/100" | bc -l)

  # User output
  echo "Running experiment:"
  echo "  Experiment: Firsttouch"
  echo "  Ratio: ${1}"
  echo "  Command: '${2}'"

  # Run 5 iters
  rm -rf results/firsttouch_${1}/
  mkdir -p results/firsttouch_${1}/
  for iter in {1..5}; do
    $SICM_DIR/deps/bin/memreserve 1 256 constant 4128116 release prefer # "Clear caches"
    cat results/pebs_128/stdout.txt | $SICM_DIR/deps/bin/memreserve 1 256 ratio ${RATIO} hold bind &>> results/firsttouch_${1}/memreserve.txt &
    sleep 5
    numastat -m &>> results/firsttouch_${1}/numastat_before.txt
    background "firsttouch_${1}" &
    background_pid=$!
    eval "env time -v numactl --preferred=1" "$2" &>> results/firsttouch_${1}/stdout.txt
    kill $background_pid
    wait $background_pid 2>/dev/null
    pkill memreserve
    sleep 5
  done
}
