#!/bin/bash

for file in `ls lulesh/lulesh_offline_pebs_*`; do
  NEWNAME=$(echo $file | awk '{gsub(/lulesh/, "roms"); print;}');
  cat $file | awk '{gsub(/\.\/lulesh2.0 -s 220 -i 20 -r 11 -b 0 -c 64 -p/, "./roms < ocean_benchmark3.in"); gsub(/high\/lulesh/, "high/roms/run"); gsub(/lulesh/, "roms"); print;}' > $NEWNAME;
  chmod +x $NEWNAME;
done
