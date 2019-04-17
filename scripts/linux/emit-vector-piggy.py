#
# This python script auto-generates the files
#
#   src/linux/ScalablePiggy.h and
#   src/linux/ScalablePiggy.S
#
# Conditions under which you need to re-run this script are
#
# * You add a bitmap
# * You add a resolution

import os
import re
import sys

assets_path = "assets/original-vector/SVG/exported"

source_file = open(sys.argv[1] + "/src/linux/ScalablePiggy.S", "w")
source_file.write("""# THIS IS AN AUTOMATICALLY GENERATED FILE. DO NOT EDIT IT.
#
# If you need to modify this file, please read the comment
# in scripts/linux/emit-vector-piggy.py

    .section ".rodata", "a"

memorySVGListStart:
    .globl memorySVGListStart

""")

header_file = open(sys.argv[1] + "/src/linux/ScalablePiggy.h", "w")
header_file.write("""/*
** THIS IS AN AUTOMATICALLY GENERATED FILE. DO NOT EDIT IT.
**
** If you need to modify this file, please read the comment
** in scripts/linux/emit-vector-piggy.py
*/

struct MemorySVG {
    const char *name;
    unsigned long size;
    unsigned long offset;
};

extern unsigned char memorySVGListStart[];

static const struct MemorySVG memorySVGList[] = {
""")

offset = 0

for name in os.listdir(sys.argv[1] + "/" + assets_path):
    if not re.match('bmp00(\\d+)(.*).svg', name):
        continue

    path = os.path.join(assets_path, name)
    size = os.stat(sys.argv[1] + "/" + path).st_size;

    source_file.write('    .incbin "../../' + path + '"' + os.linesep)
    header_file.write('     {"svg/' + name + '", ' + str(size) + ', ' +
                      str(offset) + '},' + os.linesep)

    offset += size


header_file.write("""    {NULL, 0}
};
""")
