#!/usr/bin/bash
module load gcc/9.3.0
module load cuda/10.1

output=$(numactl -H | grep -h "cpus" | tr ' ' '\n')	#identifying the cpu sets from numactl
flag1="false"
flag2="false"
flag3="false"
prev=""
declare -A node_map
node=-1
f_cpu=-1
l_cpu=-1
for i in $output
do
  if [ "$i" == "node" ]; then
	flag1="false"	
	if [ "${f_cpu}" != -1 ] && [ "${l_cpu}" != -1 ]; then
		node_map["${f_cpu}-${l_cpu}"]=$(($l_cpu-$f_cpu+1))
		f_cpu=-1
		l_cpu=-1
	fi
  elif [ "$prev" == "cpus:" ]; then
	f_cpu=$i
  elif [ "$i" == "cpus:" ]; then
	flag1="true"
  elif [ "$flag1" == "true" ]; then
	if [ "$(($i-1))" == "$prev"  ]; then
		l_cpu=$i
	else
		if [ "${f_cpu}" != -1 ] && [ "${l_cpu}" != -1 ]; then
			node_map["${f_cpu}-${l_cpu}"]=$(($l_cpu-$f_cpu+1))
			f_cpu=$i
		fi
	fi
  elif [ "$prev" == "node" ]; then
	node=$i
  fi
  prev=$i
done

if [ "${f_cpu}" != -1 ] && [ "${l_cpu}" != -1 ]; then
  node_map["${f_cpu}-${l_cpu}"]=$(($l_cpu-$f_cpu+1))
fi

out=$(make clean)
out=$(make)

for j in ${!node_map[@]}
do
	thr=${node_map[$j]}
	export OMP_NUM_THREADS="$thr"
	numactl -C "$j" ./pgmg "$j"
done

