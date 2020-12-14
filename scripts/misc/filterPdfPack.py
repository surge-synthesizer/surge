import pandas as pd
from io import StringIO
import re
from matplotlib.backends.backend_pdf import PdfPages
import matplotlib.pyplot as plt
import sys

if len(sys.argv) != 3:
    print( "Usage: python3 filterPdfPack.py infile outfile\n")
    exit(1)

fn = sys.argv[1]
outf = sys.argv[2]

with open( fn, 'r') as f:
    ct = f.readlines()

sections = []
currsect = { 'data': [] }

def tag(s):
    return re.match(r'\[([^\]]+)\]', s ).group(1)


def detag(s):
    return re.sub( r'\[[^\]]+\]', '', s )


for l in ct:
    if l.startswith("#"):
        continue

    if l.startswith("[BEGIN]"):
        currsect = { 'data' : [] }
        continue
    if l.startswith( "[END]"):
        sections.append(currsect)
        currsect = { 'data' : [] }
        continue

    if l.startswith( "[" ):
        q = detag(l)
        r = tag(l)
        currsect[r] = q
        continue

    currsect['data'].append(l)


# Create the PdfPages object to which we will save the pages:
# The with statement makes sure that the PdfPages object is closed properly at
# the end of the block, even if an Exception occurs.
with PdfPages(outf) as pdf:

    for sect in sections:
        plt.figure( figsize=(5,5))
        ds = StringIO("".join(sect['data']))
        df = pd.read_csv( ds )

        ly = False
        if( 'YLOG' in sect ):
            ly = True
        df.plot(0, logy = ly, title=sect['SECTION'], ylabel = sect['YLAB'])
        pdf.savefig()
        plt.close()
