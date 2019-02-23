#!/bin/bash

# preflight check

if [[ ! -f ./make_deb.sh ]]; then
	echo "You must run this script from within the installer_osx directory!"
	exit 1
fi

# version

if [ "$SURGE_VERSION" != "" ]; then
    VERSION="$SURGE_VERSION"
    
elif [ "$1" != "" ]; then
    VERSION="$1"
fi

if [ "$VERSION" == "" ]; then
    echo "You must specify the version you are packaging!"
    echo "eg: ./make_deb.sh 1.0.6b4"
    exit 1
fi
# locations

mkdir -p surge-synthesizer/usr/lib/vst
mkdir -p surge-synthesizer/usr/lib/vst3
mkdir -p surge-synthesizer/usr/share/surge-synthesizer
mkdir -p surge-synthesizer/DEBIAN

# build control file

if [[ -f surge-synthesizer/DEBIAN/control ]]; then
	rm surge-synthesizer/DEBIAN/control
fi
touch surge-synthesizer/DEBIAN/control
cat <<EOT >> surge-synthesizer/DEBIAN/control
Package: surge-synthesizer
Version: $VERSION
Architecture: amd64
Maintainer: surgeteam
Depends:libcairo2, libxkbcommon-x11-0, libxcb-util1, libxcb-cursor0
Provides: vst-plugin
Section: sound
Priority: optional
Description: Surge Synthesizer plugin
EOT

#copy data and vst plugins

cp -r ../resources/data/* surge-synthesizer/usr/share/surge-synthesizer/
cp ../target/vst2/Release/Surge.so surge-synthesizer/usr/lib/vst/surge-synthesizer.so
cp ../target/vst3/Release/Surge.so surge-synthesizer/usr/lib/vst3/surge-synthesizer.so

#build package

dpkg-deb --build surge-synthesizer