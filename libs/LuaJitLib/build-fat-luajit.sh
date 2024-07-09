#!/bin/bash

BD=$1
if [[ -z ${BD} ]]; then
    echo "Usage: build-linux-luajit BUILDDIR"
    exit 1
fi
echo "Building in ${BD}"

OD=${BD}/bin
SD=${BD}/src

rm -rf "${BD}"
mkdir -p "${BD}"

mkdir -p "${OD}/arm64"
mkdir "${OD}/x86_64"

mkdir "${SD}"
cp -r LuaJIT "${SD}"

cd "${SD}/LuaJIT/src"

MACOSX_DEPLOYMENT_TARGET=10.9 make amalg -j HOST_CC="clang -target `uname -m`-apple-macos10.9" TARGET_CC="xcrun --toolchain arm64 clang -target arm64-apply-macos10.9 -isysroot $(xcrun --sdk macosx --show-sdk-path) -fvisibility=hidden -fvisibility-inlines-hidden" TARGET_CFLAGS="-O3" || echo "That's OK though"
cp lib*a "${OD}/arm64"

MACOSX_DEPLOYMENT_TARGET=10.9 make clean
MACOSX_DEPLOYMENT_TARGET=10.9 make amalg -j HOST_CC="clang -target `uname -m`-apple-macos10.9" TARGET_CC="xcrun --toolchain x86_64 clang -target x86_64-apply-macos10.9 -isysroot $(xcrun --sdk macosx --show-sdk-path) -fvisibility=hidden -fvisibility-inlines-hidden" TARGET_CFLAGS="-O3" || echo "That's OK though"
cp lib*a "${OD}/x86_64"

lipo -create -arch arm64 "${OD}/arm64/libluajit.a" -arch x86_64 "${OD}/x86_64/libluajit.a" -output "${OD}/libluajit.a" || exit 17
ls -al ${OD}

exit 0