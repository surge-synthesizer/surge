#!/usr/bin/env python3

import sys
import struct
import xml.dom.minidom

if(len(sys.argv) != 2):
    print("Usage: patch-tool.py <patch-file>")
    sys.exit(1)

patch = sys.argv[1]
print("Patch: {0}".format(patch))

with open(patch, mode='rb') as patchFile:
    patchContent = patchFile.read()

fxpHeader = struct.unpack(">4si4siiii28si", patchContent[:60])
(chunkmagic, byteSize, fxMagic, version, fxId, fxVersion, numPrograms, prgName, chunkSize) = fxpHeader
print("Header Values: {0}".format(fxpHeader))


patchHeader = struct.unpack("<4siiiiiii", patchContent[60:92])
xmlsize = patchHeader[1]
print("Patch Header Values: {0}".format(patchHeader))
xmlct = patchContent[92:(92 + xmlsize)].decode('utf-8')

dom = xml.dom.minidom.parseString(xmlct)
pretty_xml_as_string = dom.toprettyxml()
print(pretty_xml_as_string)

osc = 0
scene = "A"
for i in patchHeader[2:]:
    if(i != 0):
        print("{0} osc{1} wt data size {2}".format(osc, scene, i))
    else:
        print("{0} osc{1} no wt".format(osc, scene))
    osc = osc + 1
    if(osc == 3):
        osc = 0
        scene = "B"
