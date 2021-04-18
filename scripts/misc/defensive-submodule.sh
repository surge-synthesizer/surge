#!/bin/sh
#
# This is a more defensive form of submodule init. For some reason the
# azure pipelines started failing on win and lin around dec 3 with
# RPC errors so this uses smaller chunks and tries a variety of times

git config --global http.postBuffer 524288000
grep path .gitmodules | awk '{ print $3 }' | xargs -n 1 git submodule init
grep path .gitmodules | awk '{ print $3 }' | xargs -n 1 git submodule update --depth=1
grep path .gitmodules | awk '{ print $3 }' | xargs -n 1 git submodule update --recursive --init --depth=1 --jobs=5

 

