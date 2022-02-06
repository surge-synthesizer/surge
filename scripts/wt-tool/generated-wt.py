# Generate WT files from various formulae

import math
import numpy as np
import matplotlib.pyplot as plt


def savewt(fn, samples):
    with open(fn, "wb") as outf:
        outf.write(b'vawt')
        outf.write(int(len(samples[0]) / 2).to_bytes(4, byteorder='little'))
        outf.write(len(samples).to_bytes(2, byteorder='little'))
        outf.write(bytes([4, 0]))
        for d in samples:
            outf.write(d)


def readwt(fn):
    data = []
    with open(fn, "rb") as f:
        ident = f.read(4)
        wavsz = int.from_bytes(f.read(4), 'little')
        wavct = int.from_bytes(f.read(2), 'little')
        flags = int.from_bytes(f.read(2), 'little')
        print("{} waves of size {}".format(wavct, wavsz))
        for i in range(wavct):
            raw = f.read(wavsz * 2)
            d = []
            for j in range(0, wavsz * 2, 2):
                ls = raw[j]
                ms = raw[j + 1]
                if(ms >= 128):
                    ms -= 256
                v = ls + ms * 256
                d.append(v / 32768.0)
            data.append(np.array(d))
    return data


def npftoi15bin(fsamples):
    """Given an array of float samples in range [-1,1] return an array of bytes representing the 2^15 int"""
    res = []
    for f in fsamples:
        v = f * 16384.0
        ls = int(v % 256)
        ms = int(v / 256)
        if(v < 0 and ls != 0):
            ms -= 1
        if(ls == 256):
            ls = 0
        if(ms < 0):
            ms += 256
        if(ms == 256):
            ms = 0
        res.append(ls)
        res.append(ms)
    return bytearray(res)


def generatewt(filename, genf, res, tables):
    """Given res as a function of npline,int -> npline to generate the nth table,
       generate the wt file"""
    line = np.linspace(0, 1, res, endpoint=False)
    dat = []
    for i in range(tables):
        di = genf(line, i + 1)
        dat.append(npftoi15bin(di))
    savewt(filename, dat)


def comparewt(fn1, fn2):
    d1 = readwt(fn1)
    l1 = np.linspace(0, 1, len(d1[0]), endpoint=False)

    d2 = readwt(fn2)
    l2 = np.linspace(0, 1, len(d2[0]), endpoint=False)

    if(len(d1) != len(d2)):
        print(" Lengths don't match", len(d1), " ", len(d2))
        return

    for i in range(len(d1)):
        e1 = d1[i]
        e2 = d2[i]
        plt.plot(l1, e1)
        plt.plot(l2, e2)
        plt.show()


def comparereswt(fn):
    lr = "resources/data/wavetables/" + fn + ".wt"
    hr = "resources/data/wavetables/" + fn + " HQ.wt"
    comparewt(lr, hr)


def sinepower(ls, n):
    return np.sin(2.0 * math.pi * ls)**(4 * (n - 1) + 1)


def sinepd(ls, n):
    if(n == 1):
        coeff = 1
    elif(n == 2):
        coeff = 1.5
    else:
        coeff = n - 1

    return np.sin(2.0 * math.pi * (ls ** (coeff)))


def sinehalf(ls, n):
    return np.sin(math.pi * ls * n)


def sinefm2(x, n):
    c = np.sin(2 * math.pi * x * 2)
    r = np.sin(2 * math.pi * x + (n - 1) * c / 2)
    return r


def sinefm3(x, n):
    c = np.sin(2 * math.pi * x * 3)
    r = np.sin(2 * math.pi * x + (n - 1) * c / 2)
    return r


def sinewindowed(x, n):
    xc = x - 0.5 + 1 / (2 * len(x))
    w = np.exp(-18.0 * (xc ** 2))
    r = np.sin((-1)**n * 2.0 * math.pi * n * x) * w
    return r


def squarewindowed(x, n):
    xc = x - 0.5 + 1 / (2 * len(x))
    w = np.exp(-18.0 * (xc ** 2))
    sq = np.where(np.sin((-1)**n * 2.0 * math.pi * n * x) < 0, -1, 1)
    r = sq * w
    return r


def make_high_quality_of(name, genf):
    infn = "resources/data/wavetables/" + name + ".wt"
    outfn = "resources/data/wavetables/" + name + " HQ.wt"
    ind = readwt(infn)

    generatewt(outfn, genf, 512, len(ind))


make_high_quality_of("generated/Sine PD", sinepd)
make_high_quality_of("generated/Sine Half", sinehalf)
make_high_quality_of("generated/Sine Power", sinepower)
make_high_quality_of("generated/Sine FM 2x", sinefm2)
make_high_quality_of("generated/Sine FM 3x", sinefm3)
make_high_quality_of("generated/Sine Windowed", sinewindowed)
make_high_quality_of("generated/Square Windowed", squarewindowed)
