#!/bin/bash

BD=$1
if [[ -z ${BD} ]]; then
    echo "Usage: build-linux-luajit.sh BUILDDIR"
    exit 1
fi
echo "Building in ${BD}"

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

make amalg "BUILDMODE=static" CFLAGS+=" -fPIC" || echo "That's OK though"

cp lib*a "${OD}"
echo "${OD}"
ls -al ${OD}

cp *.h "${HD}"
cp *.hpp "${HD}"
echo "${HD}"
ls -al ${HD}

exit 0