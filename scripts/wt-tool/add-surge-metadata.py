# Read the raw chunk names from a wav file

from optparse import OptionParser
import random
import os


def toInt(b):
    return int.from_bytes(b, byteorder='little')


def file_data(fn):
    with open(fn, "rb") as f:
        bdata = f.read()
    return bdata


def chunks_from_data(bdata):
    pos = 12
    res = []
    while pos < len(bdata):
        cn = bdata[pos:(pos + 4)]
        cs = toInt(bdata[(pos + 4):(pos + 8)])
        pos += cs + 8
        res.append([cn, cs])
    return res


def has_surge_chunk(bdata):
    chks = chunks_from_data(bdata)
    res = False
    for [cn, cs] in chks:
        if cn == b'srge':
            res = True
    return res


def add_surge_chunk(bdata, version, tablesize):
    newck = bytearray(16)
    newck[0] = b's'[0]
    newck[1] = b'r'[0]
    newck[2] = b'g'[0]
    newck[3] = b'e'[0]

    sz = int.to_bytes(8, 4, 'little')
    for i in range(4):
        newck[i + 4] = sz[i]

    ver = int.to_bytes(version, 4, 'little')
    for i in range(4):
        newck[i + 8] = ver[i]

    ts = int.to_bytes(tablesize, 4, 'little')
    for i in range(4):
        newck[i + 12] = ts[i]

    # OK so now we need to figure out where the fmt chunk ends
    chks = chunks_from_data(bdata)
    pos = 12
    finalpos = 0
    for [cn, cs] in chks:
        pos += cs + 8
        if (cn == b'fmt '):
            finalpos = pos
    newdata = bytearray(bdata)
    for i in range(len(newck)):
        newdata.insert(finalpos, newck[len(newck) - 1 - i])

    # Finally update the size of the file
    ns = int.to_bytes(len(newdata), 4, 'little')
    for i in range(4):
        newdata[i + 4] = ns[i]

    return newdata


def update_surge_chunk(bdata, sz):
    newdata = bytearray(bdata)
    chks = chunks_from_data(bdata)
    pos = 12
    finalpos = 0
    for [cn, cs] in chks:
        if (cn == b'srge'):
            finalpos = pos

        pos += cs + 8

    v = toInt(newdata[(finalpos + 8):(finalpos + 12)])
    s = toInt(newdata[(finalpos + 12):(finalpos + 16)])
    if(not v == 1):
        print("Not a version 1 file - v=", v, " s=", s)

    ns = int.to_bytes(sz, 4, 'little')
    for i in range(4):
        newdata[finalpos + i + 12] = ns[i]

    return newdata


def go(fn, options):
    print("Input    : ", fn)
    bdata = file_data(fn)
    sz = int(options.samples)
    print("Size     : ", sz)
    if has_surge_chunk(bdata):
        newd = update_surge_chunk(bdata, sz)
    else:
        newd = add_surge_chunk(bdata, 1, sz)

    nfn = fn + ".withsurgemeta.wav"
    if(options.out is not None):
        nfn = options.out

    if(options.inplace is not True):
        print("Output   : ", nfn)

        with open(nfn, "wb") as f:
            f.write(newd)
    else:
        print("Update in place")
        nfn = (fn + "_{}").format(random.randint(0, 1000000))
        with open(nfn, "wb") as f:
            f.write(newd)
        os.remove(fn)
        os.rename(nfn, fn)


if __name__ == "__main__":
    parser = OptionParser(usage="usage: % prog [options] inputfile")
    parser.add_option("-s", "--samples", dest="samples",
                      help="number of samples in a single table", metavar="SAMPLES")
    parser.add_option("-o", "--out", dest="out",
                      help="output file name. Default is (infile).withsurgemeta.wav", metavar="OUTFILE")
    parser.add_option("-i", "--inplace", dest="inplace", action="store_true",
                      help="Update file in place")
    (options, args) = parser.parse_args()

    print(args)
    print(options)
    if(len(args) != 1):
        parser.print_help()
        exit(1)

    go(args[0], options)
