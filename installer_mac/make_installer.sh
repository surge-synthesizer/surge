#!/bin/bash

# Documentation for pkgbuild and productbuild: https://developer.apple.com/library/archive/documentation/DeveloperTools/Reference/DistributionDefinitionRef/Chapters/Distribution_XML_Ref.html

# preflight check

if [[ ! -f ./make_installer.sh ]]; then
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
    echo "eg: ./make_installer.sh 1.0.6b4"
    exit 1
fi

# locations

PRODUCTS="../build/surge_products/"
FXPRODUCTS="../build/surge_products/"

VST3="Surge.vst3"
AU="Surge.component"
FXAU="SurgeEffectsBank.component"
FXVST3="SurgeEffectsBank.vst3"

RSRCS="../resources/data"

OUTPUT_BASE_FILENAME="Surge-$VERSION-Setup"

TARGET_DIR="installer";

build_flavor()
{
    TMPDIR=${TARGET_DIR}/tmp-pkg
    flavor=$1
    flavorprod=$2
    ident=$3
    loc=$4
    proddir=$5

    echo --- BUILDING Surge_${flavor}.pkg ---
    
    # In the past we would pkgbuild --analyze first to make a plist file, but we are perfectly fine with the
    # defaults, so we skip that step here. http://thegreyblog.blogspot.com/2014/06/os-x-creating-packages-from-command_2.html
    # was pretty handy in figuring that out and man pkgbuild convinced us to not do it, as did testing.
    #
    # The defaults only work if a component is a sole entry in a staging directory though, so synthesize that
    # by moving the product to a tmp dir

    mkdir -p $TMPDIR
    mv $proddir/$flavorprod $TMPDIR

    pkgbuild --root $TMPDIR --identifier $ident --version $VERSION --install-location $loc Surge_${flavor}.pkg || exit 1

    mv $TMPDIR/${flavorprod} $proddir
    rmdir $TMPDIR
}


# try to build VST3 package

if [[ -d $PRODUCTS/$VST3 ]]; then
    build_flavor "VST3" $VST3 "com.vemberaudio.vst3.pkg" "/Library/Audio/Plug-Ins/VST3" $PRODUCTS
fi

# try to build AU package

if [[ -d $PRODUCTS/$AU ]]; then
    build_flavor "AU" $AU "com.vemberaudio.au.pkg" "/Library/Audio/Plug-Ins/Components" $PRODUCTS
fi

# And the FXen

if [[ -d $FXPRODUCTS/$FXAU ]]; then
    build_flavor "FXAU" $FXAU "org.surge-synthesizer.fxau.pkg" "/Library/Audio/Plug-Ins/Components" $FXPRODUCTS
fi

if [[ -d $FXPRODUCTS/$FXVST3 ]]; then
    build_flavor "FXVST3" $FXVST3 "org.surge-synthesizer.fxvst3.pkg" "/Library/Audio/Plug-Ins/VST3" $FXPRODUCTS
fi

# write build info to resources folder

echo "Version: ${VERSION}" > "$RSRCS/BuildInfo.txt"
echo "Packaged on: $(date "+%Y-%m-%d %H:%M:%S")" >> "$RSRCS/BuildInfo.txt"
echo "On host: $(hostname)" >> "$RSRCS/BuildInfo.txt"
echo "Commit: $(git rev-parse HEAD)" >> "$RSRCS/BuildInfo.txt"

# build resources package

echo --- BUILDING Resources pkg ---
pkgbuild --root "$RSRCS" --identifier "com.vemberaudio.resources.pkg" --version $VERSION --scripts ResourcesPackageScript --install-location "/tmp/Surge" Surge_Resources.pkg

# remove build info from resource folder

rm "$RSRCS/BuildInfo.txt"

# create distribution.xml

if [[ -d $PRODUCTS/$VST3 ]]; then
	VST3_PKG_REF='<pkg-ref id="com.vemberaudio.vst3.pkg"/>'
	VST3_CHOICE='<line choice="com.vemberaudio.vst3.pkg"/>'
	VST3_CHOICE_DEF="<choice id=\"com.vemberaudio.vst3.pkg\" visible=\"true\" start_selected=\"true\" title=\"VST3\"><pkg-ref id=\"com.vemberaudio.vst3.pkg\"/></choice><pkg-ref id=\"com.vemberaudio.vst3.pkg\" version=\"${VERSION}\" onConclusion=\"none\">Surge_VST3.pkg</pkg-ref>"
fi
if [[ -d $PRODUCTS/$AU ]]; then
	AU_PKG_REF='<pkg-ref id="com.vemberaudio.au.pkg"/>'
	AU_CHOICE='<line choice="com.vemberaudio.au.pkg"/>'
	AU_CHOICE_DEF="<choice id=\"com.vemberaudio.au.pkg\" visible=\"true\" start_selected=\"true\" title=\"Audio Unit\"><pkg-ref id=\"com.vemberaudio.au.pkg\"/></choice><pkg-ref id=\"com.vemberaudio.au.pkg\" version=\"${VERSION}\" onConclusion=\"none\">Surge_AU.pkg</pkg-ref>"
fi
if [[ -d $FXPRODUCTS/$FXAU ]]; then
	FXAU_PKG_REF='<pkg-ref id="org.surge-synthesizer.fxau.pkg"/>'
	FXAU_CHOICE='<line choice="org.surge-synthesizer.fxau.pkg"/>'
	FXAU_CHOICE_DEF="<choice id=\"org.surge-synthesizer.fxau.pkg\" visible=\"true\" start_selected=\"true\" title=\"Audio Unit for Standalone FX\"><pkg-ref id=\"org.surge-synthesizer.fxau.pkg\"/></choice><pkg-ref id=\"org.surge-synthesizer.fxau.pkg\" version=\"${VERSION}\" onConclusion=\"none\">Surge_FXAU.pkg</pkg-ref>"
fi
if [[ -d $FXPRODUCTS/$FXVST3 ]]; then
	FXVST3_PKG_REF='<pkg-ref id="org.surge-synthesizer.fxvst3.pkg"/>'
	FXVST3_CHOICE='<line choice="org.surge-synthesizer.fxvst3.pkg"/>'
	FXVST3_CHOICE_DEF="<choice id=\"org.surge-synthesizer.fxvst3.pkg\" visible=\"true\" start_selected=\"true\" title=\"VST3 for Standalone FX\"><pkg-ref id=\"org.surge-synthesizer.fxvst3.pkg\"/></choice><pkg-ref id=\"org.surge-synthesizer.fxvst3.pkg\" version=\"${VERSION}\" onConclusion=\"none\">Surge_FXVST3.pkg</pkg-ref>"
fi

cat > distribution.xml << XMLEND
<?xml version="1.0" encoding="utf-8"?>
<installer-gui-script minSpecVersion="1">
    <title>Surge ${VERSION}</title>
    <license file="License.txt" />
    <readme file="Readme.rtf" />
    ${VST3_PKG_REF}
    ${AU_PKG_REF}
    ${FXVST3_PKG_REF}
    ${FXAU_PKG_REF}
    <pkg-ref id="com.vemberaudio.resources.pkg"/>
    <options require-scripts="false" customize="always" />
    <choices-outline>
        ${VST3_CHOICE}
        ${AU_CHOICE}
        ${FXVST3_CHOICE}
        ${FXAU_CHOICE}
        <line choice="com.vemberaudio.resources.pkg"/>
    </choices-outline>
    ${VST3_CHOICE_DEF}
    ${AU_CHOICE_DEF}
    ${FXVST3_CHOICE_DEF}
    ${FXAU_CHOICE_DEF}
    <choice id="com.vemberaudio.resources.pkg" visible="true" enabled="false" selected="true" title="Install resources">
        <pkg-ref id="com.vemberaudio.resources.pkg"/>
    </choice>
    <pkg-ref id="com.vemberaudio.resources.pkg" version="${VERSION}" onConclusion="none">Surge_Resources.pkg</pkg-ref>
</installer-gui-script>
XMLEND

# build installation bundle

if [[ ! -d ${TARGET_DIR} ]]; then
	mkdir ${TARGET_DIR}
fi
productbuild --distribution distribution.xml --package-path "./" --resources Resources "${TARGET_DIR}/$OUTPUT_BASE_FILENAME.pkg"
Rez -append Resources/icns.rsrc -o "${TARGET_DIR}/${OUTPUT_BASE_FILENAME}.pkg"
SetFile -a C "${TARGET_DIR}/${OUTPUT_BASE_FILENAME}.pkg"

# create a DMG if required

if [[ $2 == "--dmg" ]]; then
	if [[ -f "${TARGET_DIR}/$OUTPUT_BASE_FILENAME.dmg" ]]; then
		rm "${TARGET_DIR}/$OUTPUT_BASE_FILENAME.dmg"
	fi
	hdiutil create /tmp/tmp.dmg -ov -volname "$OUTPUT_BASE_FILENAME" -fs HFS+ -srcfolder "./${TARGET_DIR}/"
	hdiutil convert /tmp/tmp.dmg -format UDZO -o "${TARGET_DIR}/$OUTPUT_BASE_FILENAME.dmg"
fi

# clean up

rm distribution.xml
rm Surge_*.pkg
