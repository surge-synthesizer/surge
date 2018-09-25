#!/bin/sh
premake5 xcode4
xcodebuild build -configuration Release -project surge-vst2.xcodeproj
xcodebuild build -configuration Release -project surge-vst3.xcodeproj
xcodebuild build -configuration Release -project surge-au.xcodeproj