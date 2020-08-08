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
try:
    from io import StringIO
except ImportError:
    from StringIO import StringIO

assets_path = "assets/SurgeClassic/exported"

source_file_path = sys.argv[2] + "/ScalablePiggy.S"
header_file_path = sys.argv[2] + "/ScalablePiggy.h"

source_file = StringIO()
source_file.write(u"""# THIS IS AN AUTOMATICALLY GENERATED FILE. DO NOT EDIT IT.
#
# If you need to modify this file, please read the comment
# in scripts/linux/emit-vector-piggy.py

    .section ".rodata", "a"

memorySVGListStart:
    .globl memorySVGListStart

""")

header_file = StringIO()
header_file.write(u"""/*
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
    size = os.stat(sys.argv[1] + "/" + path).st_size

    source_file.write(u'    .incbin "%s/%s"%s' % (sys.argv[1], path, os.linesep))
    header_file.write(u'     {"svg/%s", %d, %d},%s' % (name, size, offset, os.linesep))

    offset += size


header_file.write(u"""    {NULL, 0}
};
""")


def save_if_modified(path, contents):
    try:
        same = contents == open(path, 'r').read()
    except IOError:
        same = False
    if not same:
        open(path, 'w').write(contents)
    # else:
    #     sys.stderr.write('File identical, not saving: "%s"\n' % (path))


save_if_modified(header_file_path, header_file.getvalue())
save_if_modified(source_file_path, source_file.getvalue())
