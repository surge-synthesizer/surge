#!/bin/bash

BUILD=$1

pushd /home/build/surge

cmake -B${BUILD} -GNinja -DCMAKE_BUILD_TYPE=Release -DCMAKE_CXX_COMPILER=g++-11 -DCMAKE_C_COMPILER=gcc-11
echo cmake --build ${BUILD} ${@:2}
cmake --build ${BUILD} ${@:2}

popd




