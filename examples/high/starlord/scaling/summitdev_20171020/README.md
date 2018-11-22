StarLord scaling test on Summitdev, October 20, 2017

Results were generated using weak scaling from a 128**3 problem
to a 768**3 problem. The number of GPUs mapped to the problem size as follows:

1 GPU: inputs.128
2 GPUs: inputs.192
4 GPUs: inputs.256
8 GPUs: inputs.256
16 GPUs: inputs.384
32 GPUs: inputs.512
64 GPUs: inputs.512
128 GPUs: inputs.768

For the 128 GPU problem, variability was substantial in the runtime
over the 100 steps used, so this result should be taken with a large
grain of salt.

Build line:
make -j4 COMP=PGI USE_CUDA=TRUE USE_MPI=TRUE USE_CUDA_AWARE_MPI=FALSE

StarLord version:
1d85ca9bceb7b27e0f39244ac086de50e750588e

AMReX version:
48118035fe12bfc6f2420d8b5108572f2417aa8f

Run line:
jsrun -n 1 -a 1 -g 1 ./Castro3d.pgi.MPI.CUDA.ex inputs.128 max_step=100
