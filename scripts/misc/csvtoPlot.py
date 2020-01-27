import matplotlib.pyplot as plt
import csv
import sys


inf = sys.argv[1]
outf = sys.argv[2]
title = sys.argv[3]

i = []
x = []
y = []

with open(inf, 'r') as csvfile:
    plots = csv.reader(csvfile, delimiter=',')
    for row in plots:
        i.append(int(row[0]))
        x.append(float(row[1]))
        y.append(float(row[2]))

plt.plot(i, x)
plt.plot(i, y)
plt.title(title)
plt.savefig(outf)
