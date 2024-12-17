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
mkdir -p "${OD}/arm64" || error=1
mkdir "${OD}/x86_64" || error=1
mkdir "${HD}" || error=1

# LuaJIT's top level makefile also has Darwin specific config so we build from the top source directory
cp -r LuaJIT "${SD}" || error=1
cd "${SD}/LuaJIT" || error=1

# Setting MACOSX_DEPLOYMENT_TARGET environment variable is required for building LuaJIT
export MACOSX_DEPLOYMENT_TARGET=10.9

# Pipe output to /dev/null to suppress linker errors
make amalg -j HOST_CC="clang -target `uname -m`-apple-macos10.9" TARGET_CC="xcrun --toolchain arm64 clang -target arm64-apply-macos10.9 -isysroot $(xcrun --sdk macosx --show-sdk-path) -fvisibility=hidden -fvisibility-inlines-hidden" TARGET_CFLAGS="-O3" > /dev/null 2>&1
cp src/lib*a "${OD}/arm64" || error=1

make clean || error=1
make amalg -j HOST_CC="clang -target `uname -m`-apple-macos10.9" TARGET_CC="xcrun --toolchain x86_64 clang -target x86_64-apply-macos10.9 -isysroot $(xcrun --sdk macosx --show-sdk-path) -fvisibility=hidden -fvisibility-inlines-hidden" TARGET_CFLAGS="-O3" > /dev/null 2>&1
cp src/lib*a "${OD}/x86_64" || error=1

lipo -create -arch arm64 "${OD}/arm64/libluajit.a" -arch x86_64 "${OD}/x86_64/libluajit.a" -output "${OD}/libluajit.a" || error=1

echo "${OD}"
ls -al "${OD}" || error=1

cp src/*.h "${HD}" || error=1
cp src/*.hpp "${HD}" || error=1
echo "${HD}"
ls -al "${HD}" || error=1

if [ "$error" -ne 0 ] ; then
    exit 1
fi
exit 0