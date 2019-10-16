#!/usr/bin/env bash

PARENT="$(dirname ${BASH_SOURCE[0]})"

if [[ "$#" -lt "1" ]]
then
    echo "$0 memory_type [memory_type ...]"
    echo
    echo "The order memory types should be listed, if available:"
    echo "    DRAM HBM GPU OPTANE"
    echo

    exit 1
fi

# get size of largest CPU cache in bytes
cache_size="$(cat $(for cache in /sys/devices/system/cpu/cpu0/cache/index*; do echo ${cache}; done | sort -n | tail -n 1)/size | numfmt --from=iec)"

# use a bit more data than there is cache
size=$(python -c "print ${cache_size} * 3")

# number of rounds to run
iterations="1"
export OMP_PROC_BIND=true
"${PARENT}/memory_characterization" "${size}" "${iterations}" $@
