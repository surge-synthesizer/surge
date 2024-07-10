#!/bin/bash

BD=$1
if [[ -z ${BD} ]]; then
    echo "Usage: build-mingw-luajit.sh BUILDDIR"
    exit 1
fi
echo "Building in ${BD}"

MAKE_CMD="make"
# Check if mingw32-make is available, see https://www.msys2.org/wiki/Porting/
if command -v mingw32-make &> /dev/null; then
    MAKE_CMD="mingw32-make"
fi

SD=${BD}/src
OD=${BD}/bin
HD=${BD}/include

rm -rf "${BD}"
mkdir -p "${BD}"

mkdir "${SD}"
mkdir "${OD}"
mkdir "${HD}"

cp -r LuaJIT "${SD}"
cd "${SD}/LuaJIT/src"

$MAKE_CMD amalg "BUILDMODE=static" || echo "That's OK though"

cp lib*a "${OD}"
echo "${OD}"
ls -al "${OD}"

cp *.h "${HD}"
cp *.hpp "${HD}"
echo "${HD}"
ls -al "${HD}"

exit 0