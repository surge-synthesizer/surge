#!/bin/bash

BD=$1
if [[ -z ${BD} ]]; then
    echo "Usage: build-mingw-luajit BUILDDIR"
    exit 1
fi
echo "Building in ${BD}"

MAKE_CMD="make"
# Check if mingw32-make is available
#if command -v mingw32-make &> /dev/null; then
#    MAKE_CMD="mingw32-make"
#fi

OD=${BD}/bin
SD=${BD}/src

rm -rf "${BD}"
mkdir -p "${BD}"

mkdir "${OD}"
mkdir "${SD}"

cp -r LuaJIT/ "${SD}"
cd "${SD}/LuaJIT/src"

$MAKE_CMD amalg "BUILDMODE=static" || echo "That's OK though"

mv lib*a "${OD}"
ls -al ${OD}

exit 0