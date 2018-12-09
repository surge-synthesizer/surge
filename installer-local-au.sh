#!/bin/sh

xcodebuild build -configuration Release -project surge-au.xcodeproj || exit 1
./package-au.sh
rm -rf ~/Library/Audio/Plug-Ins/Components/Surge.component
mv products/Surge.component ~/Library/Audio/Plug-Ins/Components

auval -vt aumu VmbA


