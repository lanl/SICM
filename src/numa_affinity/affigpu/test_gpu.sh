#!/usr/bin/bash

#thr=8
#export OMP_NUM_THREADS=64
./affigpu
#cp sicm_numa_config sicm_numa_config_o1
#while [ $thr -lt 97 ]; do
#	export OMP_NUM_THREADS=$thr
#	./numatest >> memory_chararcterization
#	cp sicm_numa_config "sicm_numa_config_o$thr"
#	let thr=thr+8
#done
