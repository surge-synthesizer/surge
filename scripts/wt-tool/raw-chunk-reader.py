# Read the raw chunk names from a wav file

import sys


def toInt(b):
    return int.from_bytes(b, byteorder='little')


def read_raw(fn):
    print("Opeining :", fn)
    with open(fn, "rb") as f:
        bdata = f.read()
    print("Size in bytes is", len(bdata))
    print("HEADER0: ", bdata[0:4])
    print("HEADER1: ", toInt(bdata[4:8]))
    print("HEADER2: ", bdata[8:12])
    pos = 12
    while pos < len(bdata):
        print("  CHUNK    :", bdata[pos:(pos + 4)])
        cs = toInt(bdata[(pos + 4):(pos + 8)])
        print("  CS       :", cs)
        pos += cs + 8


if __name__ == "__main__":
    read_raw(sys.argv[1])
