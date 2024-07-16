#!/bin/bash

error=0

BD="$1"
if [[ -z "${BD}" ]] ; then
    echo "Usage: build-fat-luajit.sh \"BUILDDIR\""
    exit 1
fi
echo "Building in \"${BD}\""

SD="${BD}/src"
OD="${BD}/bin"
HD="${BD}/include"

rm -rf "${BD}" || error=1
mkdir -p "${BD}" || error=1

mkdir "${SD}" || error=1
mkdir "${OD}" || error=1
mkdir "${HD}" || error=1

cp -r LuaJIT "${SD}" || error=1
cd "${SD}/LuaJIT/src" || error=1

make amalg "BUILDMODE=static" CFLAGS+=" -fPIC" || error=1

cp lib*a "${OD}" || error=1
echo "${OD}"
ls -al "${OD}" || error=1

cp *.h "${HD}" || error=1
cp *.hpp "${HD}" || error=1
echo "${HD}"
ls -al "${HD}" || error=1

if [ "$error" -ne 0 ] ; then
    exit 1
fi
exit 0