#!/bin/bash

# Documentation for pkgbuild and productbuild: https://developer.apple.com/library/archive/documentation/DeveloperTools/Reference/DistributionDefinitionRef/Chapters/Distribution_XML_Ref.html

# preflight check
INDIR=$1
SOURCEDIR=$2
TARGET_DIR=$3
VERSION=$4

TMPDIR="./installer-tmp"
mkdir -p $TMPDIR

echo "MAKE from $INDIR $SOURCEDIR into $TARGET_DIR with $VERSION"

VST3="Surge XT.vst3"
AU="Surge XT.component"
CLAP="Surge XT.clap"
APP="Surge XT.app"
LV2="Surge XT.lv2"
FXAU="Surge XT Effects.component"
FXVST3="Surge XT Effects.vst3"
FXCLAP="Surge XT Effects.clap"
FXAPP="Surge XT Effects.app"
FXLV2="Surge XT Effects.lv2"


if [ "$VERSION" == "" ]; then
    echo "You must specify the version you are packaging!"
    echo "eg: ./make_installer.sh 1.0.6b4"
    exit 1
fi


OUTPUT_BASE_FILENAME="surge-xt-macOS-$VERSION"

build_flavor()
{
    flavor=$1
    flavorprod=$2
    ident=$3
    loc=$4
    extraSigningArgs=$5

    echo --- BUILDING Surge_XT_${flavor}.pkg from "$flavorprod" ---

    workdir=$TMPDIR/$flavor
    mkdir -p $workdir

    # In the past we would pkgbuild --analyze first to make a plist file, but we are perfectly fine with the
    # defaults, so we skip that step here. http://thegreyblog.blogspot.com/2014/06/os-x-creating-packages-from-command_2.html
    # was pretty handy in figuring that out and man pkgbuild convinced us to not do it, as did testing.
    #
    # The defaults only work if a component is a sole entry in a staging directory though, so synthesize that
    # by moving the product to a tmp dir

    cp -r "$INDIR/$flavorprod" "$workdir"
    ls -l $workdir


    if [[ ! -z $MAC_SIGNING_CERT ]]; then
      [[ -z $MAC_INSTALLING_CERT ]] && echo "You need an installing cert too " && exit 2

      if [[ -d "$workdir/$flavorprod/Contents" ]]; then
        echo "Signing as a bundle"
        codesign --force -s "$MAC_SIGNING_CERT" -o runtime ${extraSigningArgs} --deep "$workdir/$flavorprod"
        codesign -vvv "$workdir/$flavorprod"
      fi
      if [[ -f "$workdir/$flavorprod/manifest.ttl" ]]; then
        echo "Signing as an LV2 ($workdir/$flavorprod)"
        ls "$workdir/$flavorprod/"
        pushd "$workdir/$flavorprod"
        codesign --force -s "$MAC_SIGNING_CERT" -o runtime *
        codesign -vvv *
        popd
      fi

      pkgbuild --sign "$MAC_INSTALLING_CERT" --root $workdir --identifier $ident --version $VERSION --install-location "$loc" "$TMPDIR/Surge_XT_${flavor}.pkg" || exit 1
      pkgutil --check-signature "$TMPDIR/Surge_XT_${flavor}.pkg"
    else
      pkgbuild --root $workdir --identifier $ident --version $VERSION --install-location "$loc" "$TMPDIR/Surge_XT_${flavor}.pkg" || exit 1
    fi

    rm -rf $workdir
}


# try to build VST3 package

if [[ -d $INDIR/$VST3 ]]; then
    build_flavor "VST3" "$VST3" "org.surge-synth-team.surge-xt.vst3.pkg" "/Library/Audio/Plug-Ins/VST3"
fi

if [[ -d $INDIR/$FXVST3 ]]; then
    build_flavor "FXVST3" "$FXVST3" "org.surge-synth-team.surge-xt-fx.vst3.pkg" "/Library/Audio/Plug-Ins/VST3"
fi

if [[ -d $INDIR/$AU ]]; then
    build_flavor "AU" "$AU" "org.surge-synth-team.surge-xt.component.pkg" "/Library/Audio/Plug-Ins/Components"
fi

if [[ -d $INDIR/$FXAU ]]; then
    build_flavor "FXAU" "$FXAU" "org.surge-synth-team.surge-xt-fx.component.pkg" "/Library/Audio/Plug-Ins/Components"
fi

if [[ -d $INDIR/$CLAP ]]; then
    build_flavor "CLAP" "$CLAP" "org.surge-synth-team.surge-xt.clap.pkg" "/Library/Audio/Plug-Ins/CLAP"
fi

if [[ -d $INDIR/$FXCLAP ]]; then
    build_flavor "FXCLAP" "$FXCLAP" "org.surge-synth-team.surge-xt-fx.clap.pkg" "/Library/Audio/Plug-Ins/CLAP"
fi

if [[ -d $INDIR/$LV2 ]]; then
    build_flavor "LV2" "$LV2" "org.surge-synth-team.surge-xt.lv2.pkg" "/Library/Audio/Plug-Ins/LV2"
fi

if [[ -d $INDIR/$FXLV2 ]]; then
    build_flavor "FXLV2" "$FXLV2" "org.surge-synth-team.surge-xt-fx.lv2.pkg" "/Library/Audio/Plug-Ins/LV2"
fi

if [[ -d $INDIR/$APP ]]; then
    build_flavor "APP" "$APP" "org.surge-synth-team.surge-xt.app.pkg" "/tmp/SXT" "--entitlements ${SOURCEDIR}/scripts/installer_mac/Resources/entitlements.plist"
fi

if [[ -d $INDIR/$FXAPP ]]; then
    build_flavor "FXAPP" "$FXAPP" "org.surge-synth-team.surge-xt-fx.app.pkg" "/tmp/SXT" "--entitlements ${SOURCEDIR}/scripts/installer_mac/Resources/entitlements.plist"
fi

# Build the resources pagkage
RSRCS=${SOURCEDIR}/resources/data
echo --- BUILDING Resources pkg ---
if [[ ! -z $MAC_INSTALLING_CERT ]]; then
  pkgbuild --sign "$MAC_INSTALLING_CERT" --root "$RSRCS" --identifier "org.surge-synth-team.surge-xt.resources.pkg" --version $VERSION --scripts ${SOURCEDIR}/scripts/installer_mac/ResourcesPackageScript --install-location "/tmp/SXT/Surge XT" ${TMPDIR}/Surge_XT_Resources.pkg
else
  pkgbuild --root "$RSRCS" --identifier "org.surge-synth-team.surge-xt.resources.pkg" --version $VERSION --scripts ${SOURCEDIR}/scripts/installer_mac/ResourcesPackageScript --install-location "/tmp/SXT/Surge XT" ${TMPDIR}/Surge_XT_Resources.pkg
fi

echo --- Sub Packages Created ---
ls -l "${TMPDIR}"

# create distribution.xml

if [[ -d $INDIR/$VST3 ]]; then
	VST3_PKG_REF='<pkg-ref id="org.surge-synth-team.surge-xt.vst3.pkg"/>'
	VST3_CHOICE='<line choice="org.surge-synth-team.surge-xt.vst3.pkg"/>'
	VST3_CHOICE_DEF="<choice id=\"org.surge-synth-team.surge-xt.vst3.pkg\" visible=\"true\" start_selected=\"true\" title=\"Surge XT VST3\"><pkg-ref id=\"org.surge-synth-team.surge-xt.vst3.pkg\"/></choice><pkg-ref id=\"org.surge-synth-team.surge-xt.vst3.pkg\" version=\"${VERSION}\" onConclusion=\"none\">Surge_XT_VST3.pkg</pkg-ref>"
fi
if [[ -d $INDIR/$AU ]]; then
	AU_PKG_REF='<pkg-ref id="org.surge-synth-team.surge-xt.component.pkg"/>'
	AU_CHOICE='<line choice="org.surge-synth-team.surge-xt.component.pkg"/>'
	AU_CHOICE_DEF="<choice id=\"org.surge-synth-team.surge-xt.component.pkg\" visible=\"true\" start_selected=\"true\" title=\"Surge XT Audio Unit\"><pkg-ref id=\"org.surge-synth-team.surge-xt.component.pkg\"/></choice><pkg-ref id=\"org.surge-synth-team.surge-xt.component.pkg\" version=\"${VERSION}\" onConclusion=\"none\">Surge_XT_AU.pkg</pkg-ref>"
fi
if [[ -d $INDIR/$CLAP ]]; then
	CLAP_PKG_REF='<pkg-ref id="org.surge-synth-team.surge-xt.clap.pkg"/>'
  CLAP_CHOICE='<line choice="org.surge-synth-team.surge-xt.clap.pkg"/>'
	CLAP_CHOICE_DEF="<choice id=\"org.surge-synth-team.surge-xt.clap.pkg\" visible=\"true\" start_selected=\"true\" title=\"Surge XT CLAP\"><pkg-ref id=\"org.surge-synth-team.surge-xt.clap.pkg\"/></choice><pkg-ref id=\"org.surge-synth-team.surge-xt.clap.pkg\" version=\"${VERSION}\" onConclusion=\"none\">Surge_XT_CLAP.pkg</pkg-ref>"
fi
if [[ -d $INDIR/$LV2 ]]; then
	LV2_PKG_REF='<pkg-ref id="org.surge-synth-team.surge-xt.lv2.pkg"/>'
  LV2_CHOICE='<line choice="org.surge-synth-team.surge-xt.lv2.pkg"/>'
	LV2_CHOICE_DEF="<choice id=\"org.surge-synth-team.surge-xt.lv2.pkg\" visible=\"true\" start_selected=\"false\" title=\"Surge XT LV2\"><pkg-ref id=\"org.surge-synth-team.surge-xt.lv2.pkg\"/></choice><pkg-ref id=\"org.surge-synth-team.surge-xt.lv2.pkg\" version=\"${VERSION}\" onConclusion=\"none\">Surge_XT_LV2.pkg</pkg-ref>"
fi
if [[ -d $INDIR/$APP ]]; then
	APP_PKG_REF='<pkg-ref id="org.surge-synth-team.surge-xt.app.pkg"/>'
	APP_CHOICE='<line choice="org.surge-synth-team.surge-xt.app.pkg"/>'
	APP_CHOICE_DEF="<choice id=\"org.surge-synth-team.surge-xt.app.pkg\" visible=\"true\" start_selected=\"true\" title=\"Surge XT App\"><pkg-ref id=\"org.surge-synth-team.surge-xt.app.pkg\"/></choice><pkg-ref id=\"org.surge-synth-team.surge-xt.app.pkg\" version=\"${VERSION}\" onConclusion=\"none\">Surge_XT_APP.pkg</pkg-ref>"
fi

if [[ -d $INDIR/$FXVST3 ]]; then
	FXVST3_PKG_REF='<pkg-ref id="org.surge-synth-team.surge-xt-fx.vst3.pkg"/>'
	FXVST3_CHOICE='<line choice="org.surge-synth-team.surge-xt-fx.vst3.pkg"/>'
	FXVST3_CHOICE_DEF="<choice id=\"org.surge-synth-team.surge-xt-fx.vst3.pkg\" visible=\"true\" start_selected=\"true\" title=\"Surge XT Effects VST3\"><pkg-ref id=\"org.surge-synth-team.surge-xt-fx.vst3.pkg\"/></choice><pkg-ref id=\"org.surge-synth-team.surge-xt-fx.vst3.pkg\" version=\"${VERSION}\" onConclusion=\"none\">Surge_XT_FXVST3.pkg</pkg-ref>"
fi
if [[ -d $INDIR/$FXAU ]]; then
	FXAU_PKG_REF='<pkg-ref id="org.surge-synth-team.surge-xt-fx.component.pkg"/>'
	FXAU_CHOICE='<line choice="org.surge-synth-team.surge-xt-fx.component.pkg"/>'
	FXAU_CHOICE_DEF="<choice id=\"org.surge-synth-team.surge-xt-fx.component.pkg\" visible=\"true\" start_selected=\"true\" title=\"Surge XT Effects Audio Unit\"><pkg-ref id=\"org.surge-synth-team.surge-xt-fx.component.pkg\"/></choice><pkg-ref id=\"org.surge-synth-team.surge-xt-fx.component.pkg\" version=\"${VERSION}\" onConclusion=\"none\">Surge_XT_FXAU.pkg</pkg-ref>"
fi
if [[ -d $INDIR/$FXCLAP ]]; then
	FXCLAP_PKG_REF='<pkg-ref id="org.surge-synth-team.surge-xt-fx.clap.pkg"/>'
	FXCLAP_CHOICE='<line choice="org.surge-synth-team.surge-xt-fx.clap.pkg"/>'
	FXCLAP_CHOICE_DEF="<choice id=\"org.surge-synth-team.surge-xt-fx.clap.pkg\" visible=\"true\" start_selected=\"true\" title=\"Surge XT Effects CLAP\"><pkg-ref id=\"org.surge-synth-team.surge-xt-fx.clap.pkg\"/></choice><pkg-ref id=\"org.surge-synth-team.surge-xt-fx.clap.pkg\" version=\"${VERSION}\" onConclusion=\"none\">Surge_XT_FXCLAP.pkg</pkg-ref>"
fi
if [[ -d $INDIR/$FXLV2 ]]; then
	FXLV2_PKG_REF='<pkg-ref id="org.surge-synth-team.surge-xt-fx.lv2.pkg"/>'
	FXLV2_CHOICE='<line choice="org.surge-synth-team.surge-xt-fx.lv2.pkg"/>'
	FXLV2_CHOICE_DEF="<choice id=\"org.surge-synth-team.surge-xt-fx.lv2.pkg\" visible=\"true\" start_selected=\"false\" title=\"Surge XT Effects LV2\"><pkg-ref id=\"org.surge-synth-team.surge-xt-fx.lv2.pkg\"/></choice><pkg-ref id=\"org.surge-synth-team.surge-xt-fx.lv2.pkg\" version=\"${VERSION}\" onConclusion=\"none\">Surge_XT_FXLV2.pkg</pkg-ref>"
fi
if [[ -d $INDIR/$FXAPP ]]; then
	FXAPP_PKG_REF='<pkg-ref id="org.surge-synth-team.surge-xt-fx.app.pkg"/>'
	FXAPP_CHOICE='<line choice="org.surge-synth-team.surge-xt-fx.app.pkg"/>'
	FXAPP_CHOICE_DEF="<choice id=\"org.surge-synth-team.surge-xt-fx.app.pkg\" visible=\"true\" start_selected=\"true\" title=\"Surge XT Effects App\"><pkg-ref id=\"org.surge-synth-team.surge-xt-fx.app.pkg\"/></choice><pkg-ref id=\"org.surge-synth-team.surge-xt-fx.app.pkg\" version=\"${VERSION}\" onConclusion=\"none\">Surge_XT_FXAPP.pkg</pkg-ref>"
fi

cat > $TMPDIR/distribution.xml << XMLEND
<?xml version="1.0" encoding="utf-8"?>
<installer-gui-script minSpecVersion="1">
    <title>Surge ${VERSION}</title>
    <license file="License.txt" />
    <readme file="Readme.rtf" />

    ${APP_PKG_REF}
    ${AU_PKG_REF}
    ${CLAP_PKG_REF}
    ${LV2_PKG_REF}
    ${VST3_PKG_REF}

    ${FXAPP_PKG_REF}
    ${FXAU_PKG_REF}
    ${FXCLAP_PKG_REF}
    ${FXLV2_PKG_REF}
    ${FXVST3_PKG_REF}
    <pkg-ref id="org.surge-synth-team.surge-xt.resources.pkg"/>
    <options require-scripts="false" customize="always" hostArchitectures="x86_64,arm64" rootVolumeOnly="true"/>
    <domains enable_anywhere="false" enable_currentUserHome="false" enable_localSystem="true"/>
    <choices-outline>
        ${APP_CHOICE}
        ${AU_CHOICE}
        ${CLAP_CHOICE}
        ${LV2_CHOICE}
        ${VST3_CHOICE}

        ${FXAPP_CHOICE}
        ${FXAU_CHOICE}
        ${FXCLAP_CHOICE}
        ${FXLV2_CHOICE}
        ${FXVST3_CHOICE}
        <line choice="org.surge-synth-team.surge-xt.resources.pkg"/>
    </choices-outline>
    ${VST3_CHOICE_DEF}
    ${AU_CHOICE_DEF}
    ${CLAP_CHOICE_DEF}
    ${LV2_CHOICE_DEF}
    ${APP_CHOICE_DEF}
    ${FXVST3_CHOICE_DEF}
    ${FXAU_CHOICE_DEF}
    ${FXCLAP_CHOICE_DEF}
    ${FXLV2_CHOICE_DEF}
    ${FXAPP_CHOICE_DEF}
    <choice id="org.surge-synth-team.surge-xt.resources.pkg" visible="true" enabled="false" selected="true" title="Install resources">
        <pkg-ref id="org.surge-synth-team.surge-xt.resources.pkg"/>
    </choice>
    <pkg-ref id="org.surge-synth-team.surge-xt.resources.pkg" version="${VERSION}" onConclusion="none">Surge_XT_Resources.pkg</pkg-ref>
</installer-gui-script>
XMLEND

# build installation bundle

pushd ${TMPDIR}
if [[ ! -z $MAC_INSTALLING_CERT ]]; then
  echo "Building SIGNED PKG"
  productbuild --sign "$MAC_INSTALLING_CERT" --distribution "distribution.xml" --package-path "." --resources ${SOURCEDIR}/scripts/installer_mac/Resources "$OUTPUT_BASE_FILENAME.pkg"
else
  echo "Building UNSIGNED PKG"
  productbuild --distribution "distribution.xml" --package-path "." --resources ${SOURCEDIR}/scripts/installer_mac/Resources "$OUTPUT_BASE_FILENAME.pkg"
fi

popd

Rez -append ${SOURCEDIR}/scripts/installer_mac/Resources/icns.rsrc -o "${TMPDIR}/${OUTPUT_BASE_FILENAME}.pkg"
SetFile -a C "${TMPDIR}/${OUTPUT_BASE_FILENAME}.pkg"
mkdir "${TMPDIR}/Surge XT"
mv "${TMPDIR}/${OUTPUT_BASE_FILENAME}.pkg" "${TMPDIR}/Surge XT"
# create a DMG if required

if [[ -f "${TARGET_DIR}/$OUTPUT_BASE_FILENAME.dmg" ]]; then
  rm "${TARGET_DIR}/$OUTPUT_BASE_FILENAME.dmg"
fi
hdiutil create /tmp/tmp.dmg -ov -volname "$OUTPUT_BASE_FILENAME" -fs HFS+ -srcfolder "${TMPDIR}/Surge XT/"
hdiutil convert /tmp/tmp.dmg -format UDZO -o "${TARGET_DIR}/$OUTPUT_BASE_FILENAME.dmg"

if [[ ! -z $MAC_SIGNING_CERT ]]; then
  codesign --force -s "$MAC_SIGNING_CERT" --timestamp "${TARGET_DIR}/$OUTPUT_BASE_FILENAME.dmg"
  codesign -vvv "${TARGET_DIR}/$OUTPUT_BASE_FILENAME.dmg"
  xcrun notarytool submit "${TARGET_DIR}/$OUTPUT_BASE_FILENAME.dmg" --apple-id ${MAC_SIGNING_ID} --team-id ${MAC_SIGNING_TEAM} --password ${MAC_SIGNING_1UPW} --wait
  xcrun stapler staple "${TARGET_DIR}/${OUTPUT_BASE_FILENAME}.dmg"
fi

# clean up

#rm distribution.xml
#rm Surge_*.pkg
