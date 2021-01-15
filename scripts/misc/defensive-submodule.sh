#!/bin/sh
#
# This is a more defensive form of submodule init. For some reason the
# azure pipelines started failing on win and lin around dec 3 with
# RPC errors so this uses smaller chunks and tries a variety of times

git config --global http.postBuffer 524288000
grep path .gitmodules | awk '{ print $3 }' | xargs -n 1 git submodule init

pushd vst3sdk 
grep path .gitmodules | awk '{ print $3 }' | xargs -n 1 git submodule init
popd

grep path .gitmodules | awk '{ print $3 }' | xargs -n 1 git submodule update --depth=1

pushd vst3sdk 
grep path .gitmodules | awk '{ print $3 }' | xargs -n 1 git submodule update --depth=1 
popd

grep path .gitmodules | awk '{ print $3 }' | xargs -n 1 git submodule update --recursive --init --depth=1 --jobs=5

export JUCE_V=6.0.7
mkdir -p libs/juce-${JUCE_V}/juce
git clone git://github.com/juce-framework/JUCE libs/juce-${JUCE_V}/juce --depth=1 -b ${JUCE_V}
 

