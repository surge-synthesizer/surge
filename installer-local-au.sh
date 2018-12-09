#!/bin/sh

xcodebuild build -configuration Release -project surge-au.xcodeproj
./package-au.sh
rm -rf ~/Library/Audio/Plug-Ins/Components/Surge.component
mv products/Surge.component ~/Library/Audio/Plug-Ins/Components


