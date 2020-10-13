#
# This python script auto-generates the files
#
#   src/common/scalableresource.h and
#   src/windows/scalableui.rc
#
# which are included by resource.h and surge.rc
# respectively. It creates those files such that
# the IDs for the scalable assets in original-vector
# are correctly ejected and refer to the correct assets
# such that IDB_BG for the 300% image is IDB_BG + SCALABLE_300_OFFSET
# or altnerately IDB_BG_SCALE_300. Both names are needed
# (alas) because .rc files cannot support identifiers which are
# operations. So we need to have an explicit define of
# a variable which has the value of the addition (which is
# what is in the scalableresource.h) and have that be consistent
# with the images and resource file definition.
#
# I chose to run this infrequently and commit the auto-generated
# output rather than run it at build time for several reasons
#
# * These files change very infrequently. In the entire surge development
#   cycle from september to january 2019 there has been only one addition
#   to the IDB list
# * Adding python as a dependency at build time is a burden on our
#   less experienced users
# * Plenty of precedent for checking in well documented generated code
#   in the audio community (juce does it).
#
# Conditions under which you need to re-run this script are
#
# * You add a bitmap
# * You change the values of the SCALABLE_ constants in resource.h
# * You add a resolution
#
# Since each of those require a commit, the committed output is also
# consistent.
#
# Finally the script is named 'emit-' rather than 'build-' or 'make-'
# to avoid tab completion finding this instead of build-win.ps1

import os, re

assetdir = "assets/SurgeClassic/exported"
svgassetdir = "assets/SurgeClassic/exported"

IDBs=[]
digitToIDB = {}
IDBtoDigit = {}
scaleToOffset = {}

scales = [ "SVG" ]
xtnToPostfix = { };


with open( "src/common/resource.h", "r" ) as r:
    for line in r:
        matches = re.match( '#define (IDB\\S+) (\\d+)', line )
        if( matches ):
            IDBs.append( matches.group(1) )
            digitToIDB[ matches.group( 2 ) ] = matches.group( 1 )
            IDBtoDigit[ matches.group( 1 ) ] = int( matches.group( 2 ) )
        matchscale = re.match( '#define SCALABLE_(\\S+)_OFFSET\\s+(\\d+)', line )
        if( matchscale ):
            scaleToOffset[ matchscale.group( 1 ) ] = int( matchscale.group( 2 ) )

subRes = open( "src/windows/scalableresource.h", "w" )
subRes.write( """
/*
** THIS IS AN AUTOMATICALLY GENERATED FILE. DO NOT EDIT IT
**
** If you need to modify this file, please read the comment
** in scripts/win/emit-vector-rc.py
**
** This file defined the constants IDB_BG_SCALE_300 and the
** like as explicit values so the .rc compiler (which cannot
** add identifiers as integers) can include assets.
**
** You can address these items as IDB_BG_SCALE_300 or
** IDB_BG + SCALE_OFFSET_300 in your non-rc code.
*/

""")
for idb in IDBs:
    subRes.write( "\n// Offset {0} by SCALABLE_100_OFFSET value and so on\n".format( idb ) )
    for sc in scales:
        line = "#define {0}_SCALE_{1} {2} \n".format(
            idb, sc, (IDBtoDigit[ idb ] + scaleToOffset[ sc ] ) )
        subRes.write( line )
subRes.close()

# Create the resource extras


# Create the SVG rc file
subrc = open( "src/windows/svgresources.rc", "w" );
subrc.write( """
/*
** THIS IS AN AUTOMATICALLY GENERATED FILE. DO NOT EDIT IT
**
** If you need to modify this file, please read the comment
** in scripts/win/emit-vector-rc.py
**
** This file imports the appropriate scaled SVG files
** for each of the identifiers in scalableresource.h.
**
** You can address these items as IDB_BG_SCALE_SVG or
** IDB_BG + SCALE_OFFSET_SVG in your non-rc code.
*/

""")

lastBase = ""
for file in os.listdir( svgassetdir ):
    if file.endswith(".svg"):
        fn = os.path.join(svgassetdir, file )
        matches = re.match( 'bmp00(.*).svg', file );
        if( matches ):
            base = digitToIDB[matches.group(1)]
            subrc.write( base + "_SCALE_SVG DATA \"" + svgassetdir + "/" + file + "\"\n" )



