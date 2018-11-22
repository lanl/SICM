# StarLord

StarLord is a stripped-down version of Castro meant to serve as a
mini-app for exploring GPU offloading.

Castro is available at:

https://github.com/AMReX-Astro/Castro

# Running on GPUs

StarLord can run some algorithms on GPUs. Some algorithms are ported using CUDA
Fortran (supported only on the PGI Fortran compiler), and others are ported
using OpenACC. Below are instructions for compiling for each:

## Compiling on bender

make CUDA_VERSION=cc60 COMPILE_CUDA_PATH=/usr/local/cuda-9.2 USE_CUDA=TRUE COMP=PGI -j 4

## Compiling on groot

make CUDA_ARCH=60 COMPILE_CUDA_PATH=/usr/local/cuda-9.2 USE_CUDA=TRUE COMP=PGI -j 4

## Compiling on Titan (OLCF)

Nothing works.

## Compiling CUDA Fortran on summitdev (OLCF)

First, swap the `xl` module for `pgi`. StarLord currently only
supports CUDA Fortran with the PGI compiler. Then load the CUDA 9 module. The latest
versions of these modules tested on summitdev are:

- pgi/17.10
- cuda/9.0.69

To compile with CUDA 8, use the following modules and also set
`CUDA_VERSION=8.0` when running `make`:

- pgi/17.9
- cuda/8.0.61-1

Then after setting the modules, in the GNUmakefile, set
`USE_CUDA=TRUE` and type `make`. The code should now compile and run.

*NOTE*: If using NVIDIA's profiling tool `nvprof` on StarLord with
CUDA 8, it will likely encounter an error on summitdev. This is an MPI
bug in CUDA 8, which is fixed in CUDA 9. To work around this problem,
compile the code with CUDA 8, but before running, swap the CUDA 8
module with CUDA 9. Now the code should run with `nvprof`.

## Running StarLord on a single GPU

On summitdev, first request a job. The following `bsub` command
requests an interactive job for 30 minutes on one node.

`bsub -P [project ID] -XF -nnodes 1 -W 30 -Is $SHELL`

Then launch StarLord using a `jsrun` command similar to the following:

`jsrun -n 1 -a 1 -g 1 ./Castro3d.pgi.CUDA.ex inputs.64`

## Running StarLord on multiple GPUs on a single node

First build StarLord with MPI support by building using the following command:

`make -j USE_MPI=TRUE`

Then to run on 4 GPUs on a single node, use the `bsub` command above with the following `jsrun` command:

`jsrun -n 4 -a 1 -g 1 ./Castro3d.pgi.MPI.CUDA.ex inputs.256`

## Running StarLord on multiple GPUs on multiple nodes

Build StarLord with MPI support as above.

Request multiple nodes using the `-nnodes` option to `bsub`.

For 1 MPI task per GPU, and e.g. 4 nodes with 4 GPUs per node, launch
StarLord via a jsrun command like:

`jsrun -n 16 -a 1 -g 1 -r 4 ./Castro3d.pgi.MPI.CUDA.ex inputs.256`

Where the '-r' option specifies the number of 1 MPI task/1 GPU
pairings (i.e. resource sets) per node.
