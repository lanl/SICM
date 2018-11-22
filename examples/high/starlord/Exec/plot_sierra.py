import matplotlib
matplotlib.use('agg')
import matplotlib.pyplot as plt
import numpy as np
import os

results_dir = 'scaling_results/'

results_files = [result for result in os.listdir(results_dir) if 'out' in result]

fom = []
num = []

for results_file in results_files:

    names = results_file.split('.')

    if not 'ngpu' in names:
        continue

    n_gpu = int(names[names.index('ngpu') + 1])

    if n_gpu not in num:
        num.append(n_gpu)
        fom.append(0.0)

    for line in open(results_dir + results_file):

        if len(line.split()) > 0 and line.split()[0] == 'Average' and line.split()[1] == 'number':

            temp_fom = float(line.split()[7])

            idx = num.index(n_gpu)
            if fom[idx] < temp_fom:
                fom[idx] = temp_fom

print(num)
print(fom)
                
n_gpus_per_node = 4
                
fom = np.array(fom)
num = np.array(num) / float(n_gpus_per_node)

fom = np.array([x for _,x in sorted(zip(num,fom))])
num = sorted(num)

print(num)
print(fom)

plt.xticks([1, 64, 128, 256, 512])
plt.tick_params(labelsize=14)

# Generate and plot the linear scaling curve

linear_scaling = np.array([fom[0] * n / num[0] for n in num])

plt.plot(num, linear_scaling, linestyle='-', lw=2,label='Linear scaling (GPUs)')

plt.plot(num, fom, marker='o', markersize=8, linestyle='--', lw=4, label='Using GPUs')

plt.xlim([0, 1.01 * num[-1]])
plt.ylim([0, 1.10 * fom[-1]])




fom = []
num = []

for results_file in results_files:

    names = results_file.split('.')

    if not 'ncpu' in names:
        continue

    n_cpu = int(names[names.index('ncpu') + 1])

    if n_cpu not in num:
        num.append(n_cpu)
        fom.append(0.0)

    for line in open(results_dir + results_file):

        if len(line.split()) > 0 and line.split()[0] == 'Average' and line.split()[1] == 'number':

            temp_fom = float(line.split()[7])

            idx = num.index(n_cpu)
            if fom[idx] < temp_fom:
                fom[idx] = temp_fom

fom = np.array(fom)
num = np.array(num)

fom = np.array([x for _,x in sorted(zip(num,fom))])
num = sorted(num)

print(num)
print(fom)

plt.plot(num, fom, marker='s', markersize=8, linestyle='--', lw=4, label='CPUs only')


plt.ylabel('Figure of Merit (zones/usec)', fontsize=20)
plt.xlabel('Number of Sierra nodes', fontsize=20)
plt.title('Weak scaling of CASTRO Sedov problem', fontsize=20)
plt.legend(loc='best')
plt.tight_layout()

plt.savefig('scaling_results/scaling.eps')
plt.savefig('scaling_results/scaling.png')
