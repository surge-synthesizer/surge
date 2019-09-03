#!/usr/bin/env python

#
# This python scripts invokes generates LV2 manifests.
# It does so by loading the plugin and invoking the adequate entry point inside
# the target directory.
#

import os
import sys
import ctypes

if len(sys.argv) != 2:
    sys.stderr.write('Please indicate the path to the Surge shared library.\n')
    sys.exit(1)

dll_path = sys.argv[1]
dll = ctypes.cdll.LoadLibrary(dll_path)

dll_dir = os.path.dirname(dll_path)
dll_filename = os.path.basename(dll_path)
dll_basename = os.path.splitext(dll_filename)[0]

os.chdir(dll_dir)
dll.lv2_generate_ttl(ctypes.c_char_p(dll_basename.encode('utf-8')))
