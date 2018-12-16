#!/bin/sh

#touch src/au/aulayer.cpp
xcodebuild build -configuration Release -project surge-vst2.xcodeproj || exit 1
./package-vst.sh
rm -rf ~/Library/Audio/Plug-Ins/VST/Surge.vst
mv products/Surge.vst ~/Library/Audio/Plug-Ins/VST

mkdir -p ~/Library/Application\ Support/Surge

echo "INSTALLING PRESETS"
(cd resources/data && tar cf - . ) | ( cd ~/Library/Application\ Support/Surge && tar xf - )

#echo "RUNNING AUVAL"
#auval -vt aumu VmbA
