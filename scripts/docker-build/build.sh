#!/bin/bash

set -x

BUILD=$1
echo "Building surge in ${BUILD}"

pushd /home/build/surge

ls -al .

cmake -B${BUILD} -GNinja -DCMAKE_BUILD_TYPE=Release -DCMAKE_CXX_COMPILER=g++-9 -DCMAKE_C_COMPILER=gcc-9 -DSURGE_BUILD_LV2=TRUE
cmake --build ${BUILD} ${@:2}

popd




