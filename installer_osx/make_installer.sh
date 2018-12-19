#!/bin/bash

# preflight check

if [[ ! -f ./make_installer.sh ]]; then
	echo "You must run this script from within the installer_osx directory!"
	exit 1
fi

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

# try to build VST2 package

if [[ -d $VST2 ]]; then
	pkgbuild --analyze --root "$VST2" Surge_VST2.plist
	pkgbuild --root "$VST2" --component-plist Surge_VST2.plist --identifier "com.vemberaudio.vst2.pkg" --version $VERSION --install-location "/Library/Audio/Plug-Ins/VST/Surge.vst" Surge_VST2.pkg
	rm Surge_VST2.plist
fi

# try to build VST3 package

if [[ -d $VST3 ]]; then
	pkgbuild --analyze --root "$VST3" Surge_VST3.plist
	pkgbuild --root "$VST3" --component-plist Surge_VST3.plist --identifier "com.vemberaudio.vst3.pkg" --version $VERSION --install-location "/Library/Audio/Plug-Ins/VST3/Surge.vst3" Surge_VST3.pkg
	rm Surge_VST3.plist
fi

# try to build AU package

if [[ -d $AU ]]; then
	pkgbuild --analyze --root "$AU" Surge_AU.plist
	pkgbuild --root "$AU" --component-plist Surge_AU.plist --identifier "com.vemberaudio.au.pkg" --version $VERSION --install-location "/Library/Audio/Plug-Ins/Components/Surge.component" Surge_AU.pkg
	rm Surge_AU.plist
fi

# write build info to resources folder

echo "Version: ${VERSION}" > "$RSRCS/BuildInfo.txt"
echo "Packaged on: $(date "+%Y-%m-%d %H:%M:%S")" >> "$RSRCS/BuildInfo.txt"
echo "On host: $(hostname)" >> "$RSRCS/BuildInfo.txt"
echo "Commit: $(git rev-parse HEAD)" >> "$RSRCS/BuildInfo.txt"

# build resources package

pkgbuild --analyze --root "$RSRCS" Surge_Resources.plist
pkgbuild --root "$RSRCS" --component-plist Surge_Resources.plist --identifier "com.vemberaudio.resources.pkg" --version $VERSION --scripts ResourcesPackageScript --install-location "/tmp/Surge" Surge_Resources.pkg
rm Surge_Resources.plist

# remove build info from resource folder

rm "$RSRCS/BuildInfo.txt"

# create distribution.xml

if [[ -d $VST2 ]]; then
	VST2_PKG_REF='<pkg-ref id="com.vemberaudio.vst2.pkg"/>'
	VST2_CHOICE='<line choice="com.vemberaudio.vst2.pkg"/>'
	VST2_CHOICE_DEF="<choice id=\"com.vemberaudio.vst2.pkg\" visible=\"true\" title=\"Install VST2\"><pkg-ref id=\"com.vemberaudio.vst2.pkg\"/></choice><pkg-ref id=\"com.vemberaudio.vst2.pkg\" version=\"${VERSION}\" onConclusion=\"none\">Surge_VST2.pkg</pkg-ref>"
fi
if [[ -d $VST3 ]]; then
	VST3_PKG_REF='<pkg-ref id="com.vemberaudio.vst3.pkg"/>'
	VST3_CHOICE='<line choice="com.vemberaudio.vst3.pkg"/>'
	VST3_CHOICE_DEF="<choice id=\"com.vemberaudio.vst3.pkg\" visible=\"true\" title=\"Install VST3\"><pkg-ref id=\"com.vemberaudio.vst3.pkg\"/></choice><pkg-ref id=\"com.vemberaudio.vst3.pkg\" version=\"${VERSION}\" onConclusion=\"none\">Surge_VST3.pkg</pkg-ref>"
fi
if [[ -d $AU ]]; then
	AU_PKG_REF='<pkg-ref id="com.vemberaudio.au.pkg"/>'
	AU_CHOICE='<line choice="com.vemberaudio.au.pkg"/>'
	AU_CHOICE_DEF="<choice id=\"com.vemberaudio.au.pkg\" visible=\"true\" title=\"Install Audio Unit\"><pkg-ref id=\"com.vemberaudio.au.pkg\"/></choice><pkg-ref id=\"com.vemberaudio.au.pkg\" version=\"${VERSION}\" onConclusion=\"none\">Surge_AU.pkg</pkg-ref>"
fi

cat > distribution.xml << XMLEND
<?xml version="1.0" encoding="utf-8"?>
<installer-gui-script minSpecVersion="1">
    <title>Install Surge ${VERSION}</title>
    <license file="License.txt" />
    ${VST2_PKG_REF}
    ${VST3_PKG_REF}
    ${AU_PKG_REF}
    <pkg-ref id="com.vemberaudio.resources.pkg"/>
    <options require-scripts="false"/>
    <choices-outline>
        ${VST2_CHOICE}
        ${VST3_CHOICE}
        ${AU_CHOICE}
        <line choice="com.vemberaudio.resources.pkg"/>
    </choices-outline>
    ${VST2_CHOICE_DEF}
    ${VST3_CHOICE_DEF}
    ${AU_CHOICE_DEF}
    <choice id="com.vemberaudio.resources.pkg" visible="false" title="Install resources">
        <pkg-ref id="com.vemberaudio.resources.pkg"/>
    </choice>
    <pkg-ref id="com.vemberaudio.resources.pkg" version="${VERSION}" onConclusion="none">Surge_Resources.pkg</pkg-ref>
</installer-gui-script>
XMLEND

# build installation bundle

if [[ ! -d installer ]]; then
	mkdir installer
fi
productbuild --distribution distribution.xml --package-path "./" --resources Resources "installer/Install Surge $VERSION.pkg"

# create a DMG if required

if [[ $2 == "--dmg" ]]; then
	if [[ -f "Install_Surge_$VERSION.dmg" ]]; then
		rm "Install_Surge_$VERSION.dmg"
	fi
	hdiutil create /tmp/tmp.dmg -ov -volname "Install Surge $VERSION" -fs HFS+ -srcfolder "./installer/" 
	hdiutil convert /tmp/tmp.dmg -format UDZO -o "Install_Surge_$VERSION.dmg"
fi

# clean up

rm distribution.xml
rm Surge_*.pkg
