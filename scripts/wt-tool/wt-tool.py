#!/usr/bin/env python3
#
# A tool for creating, destructing, and querying surge WT files
# Run wt-tool.py --help for instructions

from optparse import OptionParser
from os import listdir
from os.path import isfile, join
import wave


def read_wt_header(fn):
    with open(fn, "rb") as f:
        header = {}
        header["ident"] = f.read(4)
        # FIXME: Check that this is actually vawt

        header["wavsz"] = f.read(4)
        header["wavct"] = f.read(2)
        header["flags"] = f.read(2)

    ws = header["wavsz"]
    sz = 0
    mul = 1
    for b in ws:
        sz += b * mul
        mul *= 256

    wc = header["wavct"]
    ct = 0
    mul = 1
    for b in wc:
        ct += b * mul
        mul *= 256

    header["wavsz"] = sz
    header["wavct"] = ct

    fl = header["flags"]
    flags = {}
    flags["is_sample"] = (fl[0] & 0x01 != 0)
    flags["loop_sample"] = (fl[0] & 0x02 != 0)

    flags["format"] = "int16-full-range" if (fl[0] == 0xC) else "int16" if (fl[0] & 0x04 != 0) else "float32"
    flags["samplebytes"] = 2 if (fl[0] & 0x04 != 0) else 4

    header["flags"] = flags

    header["filesize"] = header["flags"]["samplebytes"] * header["wavsz"] * header["wavct"] + 12

    return header


def explode(fn, wav_dir):
    print("Exploding '" + fn + "' into '" + wav_dir + "'")
    header = read_wt_header(fn)
    if(header["flags"]["samplebytes"] != 2):
        raise RuntimeError("Can only handle 16 bit wt files right now")

    with open(fn, "rb") as f:
        f.read(12)
        for i in range(0, header["wavct"]):
            # 3 is fine since we are limited to 512 tables
            fn = "{0}//wt_sample_{1:03d}.wav".format(wav_dir, i)
            print("Creating " + fn)

            with wave.open(fn, "wb") as wav_file:
                wav_file.setnchannels(1)
                wav_file.setframerate(44100)
                wav_file.setsampwidth(header["flags"]["samplebytes"])
                wav_file.setnframes(header["wavsz"])

                bdata = f.read(header["wavsz"] * header["flags"]["samplebytes"])

                wav_file.writeframes(bdata)


def create(fn, wavdir, norm, intform):
    onlyfiles = [f for f in listdir(wavdir) if (isfile(join(wavdir, f)) and f.endswith(".wav"))]
    onlyfiles.sort()

    with wave.open(join(wavdir, onlyfiles[0]), "rb") as wf:
        c0 = wf.getnchannels()
        fr = wf.getframerate()
        sw = wf.getsampwidth()
        nf = wf.getnframes()

    if (c0 != 1):
        raise RuntimeError("wt-tool only processes mono inputs")
    if (fr != 44100):
        raise RuntimeError("Please use 44.1k mono 16bit PCM wav files")
    if (sw != 2):
        raise RuntimeError("Please use 44.1k mono 16bit PCM wav files")

    print("Creating '{0}' with {1} tables of length {2}".format(fn, len(onlyfiles), nf))

    databuffer = []
    for inf in onlyfiles:
        with wave.open(join(wavdir, inf), "rb") as wav_file:
            content = wav_file.readframes(nf * sw)
            databuffer.append(content)
    if (norm == "half"):
        print("Normalizing by half")
        newdb = []
        for d in databuffer:
            newrec = bytearray(len(d))

            for i in range(int(len(d) / 2)):
                ls = d[2 * i]
                ms = d[2 * i + 1]
                if(ms >= 128):
                    ms -= 256
                r = int((ls + ms * 256) / 2)

                nls = int(r % 256)
                nms = int(r / 256)

                if(r < 0 and nls != 0):
                    nms -= 1
                if(nms < 0):
                    nms += 256
                newrec[2 * i] = nls
                newrec[2 * i + 1] = nms
            newdb.append(newrec)
        databuffer = newdb
    elif (norm == "peak" or norm == "peak16"):
        print("Normalizing to " + norm)
        peakp = 0
        peakm = 0
        for d in databuffer:
            for i in range(int(len(d) / 2)):
                ls = d[2 * i]
                ms = d[2 * i + 1]
                if(ms >= 128):
                    ms -= 256
                r = int(ls + ms * 256)
                if(r > peakp):
                    peakp = r
                if(r < peakm):
                    peakm = r
        if(-peakm > peakp):
            peakp = -peakm
        print("Peak value is ", peakp)
        newdb = []

        den = 16384.0
        if (norm == "peak16"):
            den = den * 2

        for d in databuffer:
            newrec = bytearray(len(d))

            for i in range(int(len(d) / 2)):
                ls = d[2 * i]
                ms = d[2 * i + 1]
                if(ms >= 128):
                    ms -= 256
                r = int((ls + ms * 256) * den / peakp)

                nls = int(r % 256)
                nms = int(r / 256)
                if(r < 0 and nls != 0):
                    nms -= 1
                if(nms < 0):
                    nms += 256
                newrec[2 * i] = nls
                newrec[2 * i + 1] = nms
            newdb.append(newrec)
        databuffer = newdb

    else:
        print("Not applying any normalization")

    with open(fn, "wb") as outf:
        outf.write(b'vawt')
        outf.write(nf.to_bytes(4, byteorder='little'))
        outf.write((len(onlyfiles)).to_bytes(2, byteorder='little'))
        outf.write(bytes([4 if (intform == "i15") else 12, 0]))
        for d in databuffer:
            outf.write(d)


def info(fn):
    print("WT :'" + fn + "'")
    header = read_wt_header(fn)
    print("  contains  %d samples" % header["wavct"])
    print("  of length %d" % header["wavsz"])
    print("  in format %s" % header["flags"]["format"])
    if(header["flags"]["is_sample"]):
        print("   is a sample")
    if(header["flags"]["loop_sample"]):
        print("   loop_sample = true")


def main():
    parser = OptionParser(usage="usage: % prog [options]")
    parser.add_option("-a", "--action", dest="action",
                      help="undertake action. One of 'info', 'create' or 'explode'", metavar="ACTION")
    parser.add_option("-f", "--file", dest="file",
                      help="wt_file being inspected or created", metavar="FILE")
    parser.add_option("-d", "--wav_dir", dest="wav_dir",
                      help="Directory containing or receiving wav files for wt", metavar="DIR")
    parser.add_option("-n", "--normalize", dest="normalize", default="none", metavar="MODE",
                      help="""(Create only) how to normalize input.
Modes are 'none' leave input untouched;
'half' input wav are divided by 2 (so 2^16 range becomes 2^15 range);
'peak' input wav are scanned and re-peaked to 2^15.
'peak16' input wav are scanned and re-peaked to 2^15.
""")
    parser.add_option("-i", "--int-range", dest="intrange", default="i15", metavar="INTRANGE",
                      help = "Int range for creating i16 files. Either i15 or i16")
    (options, args) = parser.parse_args()

    act = options.action
    if act == "create":
        if(options.file is None or options.wav_dir is None):
            parser.print_help()
            print("\nYou must specify a file and wav_dir for create")
        else:
            create(options.file, options.wav_dir, options.normalize, options.intrange)
    elif act == "explode":
        if(options.file is None or options.wav_dir is None):
            parser.print_help()
            print("\nYou must specify a file and wav_dir for explode")
        else:
            explode(options.file, options.wav_dir)
    elif act == "info":
        if(options.file is None):
            parser.print_help()
            print("\nYou must specify a file for info")
        else:
            info(options.file)
    else:
        parser.print_help()
        print("\nUnknown action '{0}'".format(act))


if __name__ == "__main__":
    main()
