import matplotlib.pyplot as plt
import numpy as np

resultsFile = open('results.txt')

fom = []
num = []

for line in resultsFile:

    # Skip comment lines
    if line.split()[0] == '#':
        continue

    num.append(int(line.split()[0]))
    fom.append(float(line.split()[1]))

fom = np.array(fom)
num = np.array(num)

plt.xticks([1, 8, 16, 32, 64, 128])
plt.tick_params(labelsize=14)

# Generate and plot the linear scaling curve

linear_scaling = np.array([fom[0] * n / num[0] for n in num])

plt.plot(num, linear_scaling, linestyle='-', lw=2)

plt.plot(num, fom, marker='o', markersize=8, linestyle='--', lw=4)

plt.xlim([0, 1.01 * num[-1]])
plt.ylim([0, 300])

plt.ylabel('Figure of Merit (zones advanced per usec)', fontsize=16)
plt.xlabel('Number of GPUs', fontsize=16)
plt.title('Summitdev scaling results for CASTRO mini-app', fontsize=16)

plt.savefig('scaling.eps')
plt.savefig('scaling.png')

resultsFile.close()
