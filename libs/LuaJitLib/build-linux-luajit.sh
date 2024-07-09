#!/bin/bash

BD=$1
if [[ -z ${BD} ]]; then
    echo "Usage: build-mingw-luajit BUILDDIR"
    exit 1
fi
echo "Building in ${BD}"

OD=${BD}/bin
SD=${BD}/src

rm -rf "${BD}"
mkdir -p "${BD}"

mkdir "${OD}"
mkdir "${SD}"

cp -r LuaJIT/ "${SD}"
cd "${SD}/LuaJIT/src"

make amalg "BUILDMODE=static" || echo "That's OK though"

mv lib*a "${OD}"
ls -al ${OD}

exit 0