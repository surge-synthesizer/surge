#!/bin/sh

if [ ! -f vst3sdk/LICENSE.txt ]; then
  echo You have not gotten the submodules required to build Surge. Run the following command to get them.
  echo git submodule update --init --recursive
  exit
fi

premake5 xcode4
if [ -n "$VST2SDK_DIR" ]; then
	xcodebuild clean -project surge-vst2.xcodeproj
	xcodebuild build -configuration Release -project surge-vst2.xcodeproj
fi
xcodebuild clean -project surge-vst3.xcodeproj
xcodebuild build -configuration Release -project surge-vst3.xcodeproj
xcodebuild clean -project surge-au.xcodeproj
xcodebuild build -configuration Release -project surge-au.xcodeproj
