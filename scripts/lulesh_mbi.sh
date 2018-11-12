#!/bin/bash

DIR=`readlink -f ./deps`

cd examples/high/lulesh

# Full MBI run
export SH_DEFAULT_NODE="0"
export SH_PROFILE_ONE_NODE="1"
export SH_PROFILE_ONE_IMC="knl_unc_edc_eclk0,knl_unc_edc_eclk1,knl_unc_edc_eclk2,knl_unc_edc_eclk3,knl_unc_edc_eclk4,knl_unc_edc_eclk5,knl_unc_edc_eclk6,knl_unc_edc_eclk7"
export SH_ARENA_LAYOUT="SHARED_SITE_ARENAS"
export SH_PROFILE_ONE_EVENT="UNC_E_RPQ_INSERTS"
export OMP_NUM_THREADS=255
rm -rf results/mbi
mkdir -p results/mbi
for site in $(seq 1 87); do
  # Set the site that we want to isolate
  echo "$site"
  export SH_PROFILE_ONE="$site"
  sudo -E ./lulesh2.0 -s 220 -i 20 -r 11 -b 0 -c 64 -p &> results/mbi/$site.txt
done
