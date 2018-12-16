#!/bin/sh

#touch src/au/aulayer.cpp
xcodebuild build -configuration Release -project surge-vst3.xcodeproj || exit 1
./package-vst3.sh
rm -rf ~/Library/Audio/Plug-Ins/VST3/Surge.vst3
mv products/Surge.vst3 ~/Library/Audio/Plug-Ins/VST3

mkdir -p ~/Library/Application\ Support/Surge

echo "INSTALLING PRESETS"
(cd resources/data && tar cf - . ) | ( cd ~/Library/Application\ Support/Surge && tar xf - )

#echo "RUNNING AUVAL"
#auval -vt aumu VmbA
