#!/bin/bash
#
# Why does this exist? See the conversation in the CMakeLists.txt file here
#

BD=$1
if [[ -z ${BD} ]];
then
   echo "Usage: build-fat-luajit TMPDIR"
   exit 1
fi
echo "Building in ${BD}"

OD=${BD}/bin
SD=${BD}/src

mkdir -p "${BD}"
rm -rf "${OD}"
mkdir -p "${OD}/arm64"
mkdir -p "${OD}/x86_64"

mkdir "${SD}"
cp -r LuaJIT/ "${SD}"


cd "${SD}"
MACOSX_DEPLOYMENT_TARGET=10.11 make clean
MACOSX_DEPLOYMENT_TARGET=10.11 make -j HOST_CC="clang -target `uname -m`-apple-macos10.11" TARGET_CC="xcrun --toolchain arm64 clang -target arm64-apply-macos10.11 -isysroot $(xcrun --sdk macosx --show-sdk-path) -fvisibility=hidden -fvisibility-inlines-hidden" TARGET_CFLAGS="-O3"  || echo "That's OK though"
mv src/lib*a "${OD}/arm64"

MACOSX_DEPLOYMENT_TARGET=10.11 make clean
MACOSX_DEPLOYMENT_TARGET=10.11 make -j HOST_CC="clang -target `uname -m`-apple-macos10.11" TARGET_CC="xcrun --toolchain x86_64 clang -target x86_64-apply-macos10.11 -isysroot $(xcrun --sdk macosx --show-sdk-path) -fvisibility=hidden -fvisibility-inlines-hidden" TARGET_CFLAGS="-O3"  || echo "That's OK though"

mv src/lib*a "${OD}/x86_64"

lipo -create -arch arm64 "${OD}/arm64/libluajit.a" -arch x86_64 "${OD}/x86_64/libluajit.a" -output "${OD}/libluajit.a" || exit 17
ls -al ${OD}

exit 0
