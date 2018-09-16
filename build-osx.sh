#!/bin/sh
premake4 xcode4
xcodebuild build -configuration Release -project surge-vst2.xcodeproj
xcodebuild build -configuration Release -project surge-vst3.xcodeproj
xcodebuild build -configuration Release -project surge-au.xcodeproj
xcodebuild build -configuration ReleaseDemo -project surge-vst2.xcodeproj
xcodebuild build -configuration ReleaseDemo -project surge-vst3.xcodeproj
xcodebuild build -configuration ReleaseDemo -project surge-au.xcodeproj