#!/bin/bash

DIR=`readlink -f ./deps`

# Define the variables for the compiler wrappers
export LD_COMPILER="$DIR/bin/clang"
export LD_LINKER="$DIR/bin/clang"
export C_COMPILER="$DIR/bin/clang"
export LLVMLINK="$DIR/bin/llvm-link"
export OPT="$DIR/bin/opt"

# Make sure the Lulesh Makefile finds our wrappers
export COMPILER_WRAPPER="$DIR/bin/compiler_wrapper.sh -g"
export LD_WRAPPER="$DIR/bin/ld_wrapper.sh -g"
#export COMPILER_WRAPPER="$DIR/bin/clang -g"
#export LD_WRAPPER="$DIR/bin/clang -g"

# Update SICM
make #&> /dev/null
make install #&> /dev/null

# Compile Lulesh
cd examples/high/imagick
#./clean.sh
#./compile.sh

# For MBI profiling
export SH_DEFAULT_NODE="0"
#export SH_PROFILE_ONE_NODE="1"
#export SH_PROFILE_ONE_IMC="knl_unc_edc_eclk0,knl_unc_edc_eclk1,knl_unc_edc_eclk2,knl_unc_edc_eclk3,knl_unc_edc_eclk4,knl_unc_edc_eclk5,knl_unc_edc_eclk6,knl_unc_edc_eclk7"
#export SH_ARENA_LAYOUT="SHARED_SITE_ARENAS"
#export SH_PROFILE_ONE_EVENT="UNC_E_RPQ_INSERTS"

# For PEBS profiling
export SH_ARENA_LAYOUT="SHARED_SITE_ARENAS"
export SH_PROFILE_ALL="1"
export SH_MAX_SAMPLE_PAGES="512"
export SH_SAMPLE_FREQ="250"
export SH_PROFILE_RSS="1"

# Prefetching off
#sudo -E env PATH="$PATH:$HOME/msr-tools" wrmsr -a 0x1A4 0xf
sudo -E env PATH="$PATH:$HOME/msr-tools" wrmsr -a 0x1A4 0x0

# Full MBI run
export OMP_NUM_THREADS=255
rm -rf results
mkdir -p results
#for site in $(seq 1 87); do
  # Set the site that we want to isolate
#  echo "$site"
#  export SH_PROFILE_ONE="$site"
#  sudo -E ./lulesh2.0 -s 220 -i 5 -r 11 -b 0 -c 64 -p #&> results/$site.txt
#done

sudo -E time ./imagick -limit disk 0 refspeed_input.tga -resize 817% -rotate -2.76 -shave 540x375 -alpha remove -auto-level -contrast-stretch 1x1% -colorspace Lab -channel R -equalize +channel -colorspace sRGB -define histogram:unique-colors=false -adaptive-blur 0x5 -despeckle -auto-gamma -adaptive-sharpen 55 -enhance -brightness-contrast 10x10 -resize 30% refspeed_output.tga &> results/pebs.txt

# Perf
#sudo -E ../../../../pmu-tools/ocperf.py record -e mem_uops_retired.l2_miss_loads:p ./lulesh2.0 -s 220 -i 5 -r 11 -b 0 -c 64 -p
#sudo -E ../../../../pmu-tools/ocperf.py report
