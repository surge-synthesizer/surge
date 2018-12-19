#!/bin/bash

# version and locations

VERSION="$1"
VST2="../products/Surge.vst"
VST3="../products/Surge.vst3"
AU="../products/Surge.component"
RSRCS="../resources/data"

if [[ $1 == "" ]]; then
	echo "You must specify the version you are packaging!"
	echo "eg: ./make_installer.sh 1.0.6b4"
	exit 1
fi

# empty strings for the package arguments
# will set them to something if the package is present

VST2_arg=""
VST3_arg=""
AU_arg=""

# try to build VST2 package

if [[ -d $VST2 ]]; then
	pkgbuild --analyze --root "$VST2" Surge_VST2.plist
	pkgbuild --root "$VST2" --component-plist Surge_VST2.plist --identifier "com.vemberaudio.vst2.pkg" --version $VERSION --install-location "/Library/Audio/Plug-Ins/VST/Surge.vst" Surge_VST2.pkg
	VST2_arg="--package Surge_VST2.pkg"
	rm Surge_VST2.plist
fi

# try to build VST3 package

if [[ -d $VST3 ]]; then
	pkgbuild --analyze --root "$VST3" Surge_VST3.plist
	pkgbuild --root "$VST3" --component-plist Surge_VST3.plist --identifier "com.vemberaudio.vst3.pkg" --version $VERSION --install-location "/Library/Audio/Plug-Ins/VST3/Surge.vst3" Surge_VST3.pkg
	VST3_arg="--package Surge_VST3.pkg"
	rm Surge_VST3.plist
fi

# try to build AU package

if [[ -d $AU ]]; then
	pkgbuild --analyze --root "$AU" Surge_AU.plist
	pkgbuild --root "$AU" --component-plist Surge_AU.plist --identifier "com.vemberaudio.au.pkg" --version $VERSION --install-location "/Library/Audio/Plug-Ins/Components/Surge.component" Surge_AU.pkg
	AU_arg="--package Surge_AU.pkg"
	rm Surge_AU.plist
fi

# build resources package

pkgbuild --analyze --root "$RSRCS" Surge_Resources.plist
pkgbuild --root "$RSRCS" --component-plist Surge_Resources.plist --identifier "com.vemberaudio.resources.pkg" --version $VERSION --scripts ResourcesPackageScript --install-location "/tmp/Surge" Surge_Resources.pkg
rm Surge_Resources.plist

# create distribution.xml

echo '<?xml version="1.0" encoding="utf-8"?>' > distribution.xml
echo '<installer-gui-script minSpecVersion="1">' >> distribution.xml
echo "    <title>Install Surge $VERSION</title>" >> distribution.xml
echo '    <license file="License.txt" />' >> distribution.xml
if [[ -d $VST2 ]]; then
	echo '    <pkg-ref id="com.vemberaudio.vst2.pkg"/>' >> distribution.xml
fi
if [[ -d $VST3 ]]; then
	echo '    <pkg-ref id="com.vemberaudio.vst3.pkg"/>' >> distribution.xml
fi
if [[ -d $AU ]]; then
	echo '    <pkg-ref id="com.vemberaudio.au.pkg"/>' >> distribution.xml
fi
echo '    <pkg-ref id="com.vemberaudio.resources.pkg"/>' >> distribution.xml
echo '    <options require-scripts="false"/>' >> distribution.xml
echo '    <choices-outline>' >> distribution.xml
if [[ -d $VST2 ]]; then
	echo '        <line choice="com.vemberaudio.vst2.pkg"/>' >> distribution.xml
fi
if [[ -d $VST3 ]]; then
	echo '        <line choice="com.vemberaudio.vst3.pkg"/>' >> distribution.xml
fi
if [[ -d $AU ]]; then
	echo '        <line choice="com.vemberaudio.au.pkg"/>' >> distribution.xml
fi
echo '        <line choice="com.vemberaudio.resources.pkg"/>' >> distribution.xml
echo '    </choices-outline>' >> distribution.xml
if [[ -d $VST2 ]]; then
	echo '    <choice id="com.vemberaudio.vst2.pkg" visible="true" title="Install VST2">' >> distribution.xml
	echo '        <pkg-ref id="com.vemberaudio.vst2.pkg"/>' >> distribution.xml
	echo '    </choice>' >> distribution.xml
	echo -n '    <pkg-ref id="com.vemberaudio.vst2.pkg" version="' >> distribution.xml
	echo -n $VERSION >> distribution.xml
	echo '" onConclusion="none">Surge_VST2.pkg</pkg-ref>' >> distribution.xml
fi
if [[ -d $VST3 ]]; then
	echo '    <choice id="com.vemberaudio.vst3.pkg" visible="true" title="Install VST3">' >> distribution.xml
	echo '        <pkg-ref id="com.vemberaudio.vst3.pkg"/>' >> distribution.xml
	echo '    </choice>' >> distribution.xml
	echo -n '    <pkg-ref id="com.vemberaudio.vst3.pkg" version="' >> distribution.xml
	echo -n $VERSION >> distribution.xml
	echo '" onConclusion="none">Surge_VST3.pkg</pkg-ref>' >> distribution.xml
fi
if [[ -d $AU ]]; then
	echo '    <choice id="com.vemberaudio.au.pkg" visible="true" title="Install Audio Unit">' >> distribution.xml
	echo '        <pkg-ref id="com.vemberaudio.au.pkg"/>' >> distribution.xml
	echo '    </choice>' >> distribution.xml
	echo -n '    <pkg-ref id="com.vemberaudio.au.pkg" version="' >> distribution.xml
	echo -n $VERSION >> distribution.xml
	echo '" onConclusion="none">Surge_AU.pkg</pkg-ref>' >> distribution.xml
fi
echo '    <choice id="com.vemberaudio.resources.pkg" visible="false" title="Install resources">' >> distribution.xml
echo '        <pkg-ref id="com.vemberaudio.resources.pkg"/>' >> distribution.xml
echo '    </choice>' >> distribution.xml
echo -n '    <pkg-ref id="com.vemberaudio.resources.pkg" version="' >> distribution.xml
echo -n $VERSION >> distribution.xml
echo '" onConclusion="none">Surge_Resources.pkg</pkg-ref>' >> distribution.xml
echo '</installer-gui-script>' >> distribution.xml

# build installation bundle

mkdir installer
productbuild --distribution distribution.xml --package-path "./" --resources Resources "installer/Install Surge.pkg"

# clean up

rm distribution.xml
rm Surge_*.pkg
