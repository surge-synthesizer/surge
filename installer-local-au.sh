#!/bin/sh

#touch src/au/aulayer.cpp
xcodebuild build -configuration Release -project surge-au.xcodeproj || exit 1
./package-au.sh
rm -rf ~/Library/Audio/Plug-Ins/Components/Surge.component
mv products/Surge.component ~/Library/Audio/Plug-Ins/Components

mkdir -p ~/Library/Application\ Support/Surge

echo "INSTALLING PRESETS"
(cd resources/data && tar cf - . ) | ( cd ~/Library/Application\ Support/Surge && tar xf - )

echo "RUNNING AUVAL"
auval -vt aumu VmbA


