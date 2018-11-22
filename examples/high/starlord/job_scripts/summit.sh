#!/bin/bash

#BSUB -P AST106sumdev
#BSUB -J starlord
#BSUB -o starlord.o%J
#BSUB -W 30
#BSUB -n 1


# need to find a better way to specify to cd into the submit dir
#cd /lustre/atlas/scratch/$USER/ast106/testing

# we want to assign MPI tasks round-robin between sockets -- this
# ensures that if we have 4 MPI tasks on a node, each one will have
# its own GPU
#
# -map-by socket

export LD_LIBRARY_PATH=/sw/summitdev/gcc/5.4.0new/lib64/:$LD_LIBRARY_PATH

mpirun -n 1 -map-by socket -gpu ./Castro3d.pgi.TPROF.MPI.CUDA.ex inputs.128





