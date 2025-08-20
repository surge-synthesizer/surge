# Just the normalization path of the wave table code
# A bit of copy and paste going on here from wt-tool. Should fix that one day

from optparse import OptionParser
import wave


def main():
    parser = OptionParser(usage="usage: % prog [options] infile outfile")
    parser.add_option("-n", "--normalize", dest="normalize", default="half", metavar="MODE",
                      help="""(Create only) Sets how the input should be normalized. Options are:
'none' leave input untouched
'half' input WAV is divided by 2 (so 2^16 range becomes 2^15 range)
'peak' input WAV is scanned and normalized to -6 dBFS
'peak16' input WAV is scanned and normalized to 0 dBFS
""")
    (options, args) = parser.parse_args()

    if(len(args) != 2):
        print("Please specify an input and an output file!\n")
        parser.print_help()
        return

    inf = args[0]
    outf = args[1]

    with wave.open(inf, "rb") as wf:
        c0 = wf.getnchannels()
        fr = wf.getframerate()
        sw = wf.getsampwidth()
        nf = wf.getnframes()
        wavdata = wf.readframes(nf * sw * c0)

    if (c0 != 1):
        raise RuntimeError("Please use 44.1k, 16-bit, mono WAV files! This file has more than one audio channel.")
    if (fr != 44100):
        raise RuntimeError("Please use 44.1k, 16-bit, mono WAV files! This file has a different sample rate.")
    if (sw != 2):
        raise RuntimeError("Please use 44.1k, 16-bit, mono WAV files. This file is not 16-bit.")

    newrec = bytearray(len(wavdata))

    if(options.normalize == "half"):
        print("Normalizing by half")

        for i in range(int(len(wavdata) / 2)):
            ls = wavdata[2 * i]
            ms = wavdata[2 * i + 1]

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

    elif (options.normalize == "peak" or options.normalize == "peak16"):
        print("Normalizing to " + options.normalize)

        peakp = 0
        peakm = 0

        d = wavdata

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

        den = 16384.0

        if (norm == "peak16"):
            den = den * 2

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

    else:
        print(f"Unknown normalization {options.normalize}!")
        parser.print_help()

    with wave.open(outf, "wb") as wav_file:
        wav_file.setnchannels(1)
        wav_file.setframerate(44100)
        wav_file.setsampwidth(2)
        wav_file.setnframes(int(len(newrec) / 2))

        wav_file.writeframes(newrec)


if __name__ == "__main__":
    main()
