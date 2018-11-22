#!/bin/bash

amrex_server=https://github.com/AMReX-Codes
astro_server=https://github.com/AMReX-Astro
micro_server=https://github.com/StarKiller-astro

# AMReX
echo " "
echo "cloning amrex"
git clone ${amrex_server}/amrex.git
cd amrex
git checkout ${branch}
cd ..

# Microphysics
echo " "
echo "cloning Microphysics"
git clone ${micro_server}/Microphysics.git
cd Microphysics
git checkout ${branch}
cd ..
