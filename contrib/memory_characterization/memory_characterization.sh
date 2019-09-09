#!/usr/bin/env bash

PARENT="$(dirname ${BASH_SOURCE[0]})"

if [[ "$#" -lt "1" ]]
then
    echo "$0 memory_type [memory_type ...]"
    echo
    echo "The order memory types should be listed, if available:"
    echo "    HBM DRAM GPU OPTANE"
    echo
fi

# get size of largest CPU cache in bytes
cache_size="$(cat $(for cache in /sys/devices/system/cpu/cpu0/cache/index*; do echo ${cache}; done | sort -n | tail -n 1)/size | numfmt --from=iec)"
# use a bit more data than there is cache
size=$(python -c "print ${cache_size} * 3")

# number of rounds to run
iterations="1"

# build, run, delete
cd "${PARENT}"
gcc -Wall -Ikmeans memory_characterization.c kmeans/kmeans.c -lnuma -fopenmp -o memory_characterization
export OMP_PROC_BIND=true
time ./memory_characterization "${size}" "${iterations}" $@
rm -f memory_characterization
