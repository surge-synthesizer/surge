#!/usr/bin/env python
# converted from Perl script by ChatGPT, with minor tweaks (mainly the getParameterDisplay addition)

import sys
import os
import re

pl = sys.argv[1]
aw = sys.argv[2] if len(sys.argv) > 2 else "../../../airwindows/"

print(f"Grabbing '{pl}' from '{aw}'")

with open(f"{aw}/plugins/MacVST/{pl}/source/{pl}.h", "r") as f:
    with open(f"src/{pl}.h", "w") as out:
        openNS = False

        for line in f:
            line = line.replace("audioeffectx.h", "airwindows/AirWinBaseClass.h")

            if "enum" in line and not openNS:
                openNS = True
                out.write(f"namespace {pl} {{\n\n")

            if "getParameterDisplay" in line:
                line = re.sub(r"\(.*\)", "(VstInt32 index, char *text, float extVal, bool isExternal)", line)

            if "#endif" in line and openNS:
                out.write(f"}} // end namespace {pl}\n\n")

            out.write(line)

with open(f"{aw}/plugins/MacVST/{pl}/source/{pl}Proc.cpp", "r") as f:
    with open(f"src/{pl}Proc.cpp", "w") as out:
        for line in f:
            out.write(line)

            if "#endif" in line:
                out.write(f"\nnamespace {pl} {{\n\n")

        out.write(f"\n}} // end namespace {pl}\n\n")

with open(f"{aw}/plugins/MacVST/{pl}/source/{pl}.cpp", "r") as f:
    with open(f"src/{pl}.cpp", "w") as out:
        for line in f:
            if "getParameterDisplay" in line:
                line = re.sub(r"\(.*\)", "(VstInt32 index, char *text, float extVal, bool isExternal)", line)

            if "createEffectInstance" in line:
                line = "// " + line

            out.write(line)

            if "#endif" in line:
                out.write(f"\nnamespace {pl} {{\n\n")

        out.write(f"\n}} // end namespace {pl}\n\n")