#!/usr/bin/env bash

set -e

if [[ "$#" -lt 2 ]]
then
    echo "Syntax: $0 path-prefix count size [cbmax]" 1>&2
    exit 1
fi

prefix="$1"
count="$2"
size="$3"
cbmax="$4"

gnuplot <<EOF

set terminal svg size 1024,1024 font ",12"
set output '${prefix}.svg'
set multiplot layout 2,2 title "Comparison of ${count} x ${size} Allocation Moves"
set key outside right
unset key

set xlabel "Source NUMA Node"
set ylabel "Destination NUMA Node"

set cbrange [0:${cbmax}]
set cblabel "RealTime (s)"
# set cbtics .5
set format cb "%.2f"

set title "posix\\\_memalign"
plot "${prefix}.posix_memalign" matrix rowheaders columnheaders using 1:2:3 with image pixels notitle, \
     ""                         matrix rowheaders columnheaders using 1:2:(sprintf("%.2f s", \$3)) with labels notitle

set title "mmap"
plot "${prefix}.mmap" matrix rowheaders columnheaders using 1:2:3 with image pixels notitle, \
     ""               matrix rowheaders columnheaders using 1:2:(sprintf("%.2f s", \$3)) with labels notitle

set title "numa"
plot "${prefix}.numa" matrix rowheaders columnheaders using 1:2:3 with image pixels notitle, \
     ""               matrix rowheaders columnheaders using 1:2:(sprintf("%.2f s", \$3)) with labels notitle

set title "sicm"
plot "${prefix}.sicm" matrix rowheaders columnheaders using 1:2:3 with image pixels notitle, \
     ""               matrix rowheaders columnheaders using 1:2:(sprintf("%.2f s", \$3)) with labels notitle




EOF
