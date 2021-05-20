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
APP="Surge XT.app"
FXAU="Surge XT Effects.component"
FXVST3="Surge XT Effects.vst3"
FXAPP="Surge XT Effects.app"


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

    pkgbuild --root $workdir --identifier $ident --version $VERSION --install-location "$loc" "$TMPDIR/Surge_XT_${flavor}.pkg" || exit 1

    rm -rf $workdir
}


# try to build VST3 package

if [[ -d $INDIR/$VST3 ]]; then
    build_flavor "VST3" "$VST3" "com.surge-synth-team.surge-xt.vst3.pkg" "/Library/Audio/Plug-Ins/VST3"
fi

if [[ -d $INDIR/$FXVST3 ]]; then
    build_flavor "FXVST3" "$FXVST3" "com.surge-synth-team.surge-xt-fx.vst3.pkg" "/Library/Audio/Plug-Ins/VST3"
fi

if [[ -d $INDIR/$AU ]]; then
    build_flavor "AU" "$AU" "com.surge-synth-team.surge-xt.component.pkg" "/Library/Audio/Plug-Ins/Components"
fi

if [[ -d $INDIR/$FXAU ]]; then
    build_flavor "FXAU" "$FXAU" "com.surge-synth-team.surge-xt-fx.component.pkg" "/Library/Audio/Plug-Ins/Components"
fi

if [[ -d $INDIR/$APP ]]; then
    build_flavor "APP" "$APP" "com.surge-synth-team.surge-xt.app.pkg" "/Applications"
fi

if [[ -d $INDIR/$FXAPP ]]; then
    build_flavor "FXAPP" "$FXAPP" "com.surge-synth-team.surge-xt-fx.app.pkg" "/Applications"
fi

# Build the resources pagkage
RSRCS=${SOURCEDIR}/resources/data
echo --- BUILDING Resources pkg ---
pkgbuild --root "$RSRCS" --identifier "com.surge-synth-team.surge-xt.resources.pkg" --version $VERSION --scripts ${SOURCEDIR}/scripts/installer_mac/ResourcesPackageScript --install-location "/tmp/Surge XT" ${TMPDIR}/Surge_XT_Resources.pkg

echo --- Sub Packages Created ---
ls -l "${TMPDIR}"

# create distribution.xml

if [[ -d $INDIR/$VST3 ]]; then
	VST3_PKG_REF='<pkg-ref id="com.surge-synth-team.surge-xt.vst3.pkg"/>'
	VST3_CHOICE='<line choice="com.surge-synth-team.surge-xt.vst3.pkg"/>'
	VST3_CHOICE_DEF="<choice id=\"com.surge-synth-team.surge-xt.vst3.pkg\" visible=\"true\" start_selected=\"true\" title=\"Surge XT VST3\"><pkg-ref id=\"com.surge-synth-team.surge-xt.vst3.pkg\"/></choice><pkg-ref id=\"com.surge-synth-team.surge-xt.vst3.pkg\" version=\"${VERSION}\" onConclusion=\"none\">Surge_XT_VST3.pkg</pkg-ref>"
fi
if [[ -d $INDIR/$AU ]]; then
	AU_PKG_REF='<pkg-ref id="com.surge-synth-team.surge-xt.component.pkg"/>'
	AU_CHOICE='<line choice="com.surge-synth-team.surge-xt.component.pkg"/>'
	AU_CHOICE_DEF="<choice id=\"com.surge-synth-team.surge-xt.component.pkg\" visible=\"true\" start_selected=\"true\" title=\"Surge XT Audio Unit\"><pkg-ref id=\"com.surge-synth-team.surge-xt.component.pkg\"/></choice><pkg-ref id=\"com.surge-synth-team.surge-xt.component.pkg\" version=\"${VERSION}\" onConclusion=\"none\">Surge_XT_AU.pkg</pkg-ref>"
fi
if [[ -d $INDIR/$APP ]]; then
	APP_PKG_REF='<pkg-ref id="com.surge-synth-team.surge-xt.app.pkg"/>'
	APP_CHOICE='<line choice="com.surge-synth-team.surge-xt.app.pkg"/>'
	APP_CHOICE_DEF="<choice id=\"com.surge-synth-team.surge-xt.app.pkg\" visible=\"true\" start_selected=\"true\" title=\"Surge XT App\"><pkg-ref id=\"com.surge-synth-team.surge-xt.app.pkg\"/></choice><pkg-ref id=\"com.surge-synth-team.surge-xt.app.pkg\" version=\"${VERSION}\" onConclusion=\"none\">Surge_XT_APP.pkg</pkg-ref>"
fi

if [[ -d $INDIR/$FXVST3 ]]; then
	FXVST3_PKG_REF='<pkg-ref id="com.surge-synth-team.surge-xt-fx.vst3.pkg"/>'
	FXVST3_CHOICE='<line choice="com.surge-synth-team.surge-xt-fx.vst3.pkg"/>'
	FXVST3_CHOICE_DEF="<choice id=\"com.surge-synth-team.surge-xt-fx.vst3.pkg\" visible=\"true\" start_selected=\"true\" title=\"Surge XT Effects Bank VST3\"><pkg-ref id=\"com.surge-synth-team.surge-xt-fx.vst3.pkg\"/></choice><pkg-ref id=\"com.surge-synth-team.surge-xt-fx.vst3.pkg\" version=\"${VERSION}\" onConclusion=\"none\">Surge_XT_FXVST3.pkg</pkg-ref>"
fi
if [[ -d $INDIR/$FXAU ]]; then
	FXAU_PKG_REF='<pkg-ref id="com.surge-synth-team.surge-xt-fx.component.pkg"/>'
	FXAU_CHOICE='<line choice="com.surge-synth-team.surge-xt-fx.component.pkg"/>'
	FXAU_CHOICE_DEF="<choice id=\"com.surge-synth-team.surge-xt-fx.component.pkg\" visible=\"true\" start_selected=\"true\" title=\"Surge XT Effects Bank Audio Unit\"><pkg-ref id=\"com.surge-synth-team.surge-xt-fx.component.pkg\"/></choice><pkg-ref id=\"com.surge-synth-team.surge-xt-fx.component.pkg\" version=\"${VERSION}\" onConclusion=\"none\">Surge_XT_FXAU.pkg</pkg-ref>"
fi
if [[ -d $INDIR/$FXAPP ]]; then
	FXAPP_PKG_REF='<pkg-ref id="com.surge-synth-team.surge-xt-fx.app.pkg"/>'
	FXAPP_CHOICE='<line choice="com.surge-synth-team.surge-xt-fx.app.pkg"/>'
	FXAPP_CHOICE_DEF="<choice id=\"com.surge-synth-team.surge-xt-fx.app.pkg\" visible=\"true\" start_selected=\"true\" title=\"Surge XT Effects Bank App\"><pkg-ref id=\"com.surge-synth-team.surge-xt-fx.app.pkg\"/></choice><pkg-ref id=\"com.surge-synth-team.surge-xt-fx.app.pkg\" version=\"${VERSION}\" onConclusion=\"none\">Surge_XT_FXAPP.pkg</pkg-ref>"
fi

cat > $TMPDIR/distribution.xml << XMLEND
<?xml version="1.0" encoding="utf-8"?>
<installer-gui-script minSpecVersion="1">
    <title>Surge ${VERSION}</title>
    <license file="License.txt" />
    <readme file="Readme.rtf" />
    ${VST3_PKG_REF}
    ${AU_PKG_REF}
    ${APP_PKG_REF}
    ${FXVST3_PKG_REF}
    ${FXAU_PKG_REF}
    ${FXAPP_PKG_REF}
    <pkg-ref id="com.surge-synth-team.surge-xt.resources.pkg"/>
    <options require-scripts="false" customize="always" />
    <choices-outline>
        ${VST3_CHOICE}
        ${AU_CHOICE}
        ${APP_CHOICE}
        ${FXVST3_CHOICE}
        ${FXAU_CHOICE}
        ${FXAPP_CHOICE}
        <line choice="com.surge-synth-team.surge-xt.resources.pkg"/>
    </choices-outline>
    ${VST3_CHOICE_DEF}
    ${AU_CHOICE_DEF}
    ${APP_CHOICE_DEF}
    ${FXVST3_CHOICE_DEF}
    ${FXAU_CHOICE_DEF}
    ${FXAPP_CHOICE_DEF}
    <choice id="com.surge-synth-team.surge-xt.resources.pkg" visible="true" enabled="false" selected="true" title="Install resources">
        <pkg-ref id="com.surge-synth-team.surge-xt.resources.pkg"/>
    </choice>
    <pkg-ref id="com.surge-synth-team.surge-xt.resources.pkg" version="${VERSION}" onConclusion="none">Surge_XT_Resources.pkg</pkg-ref>
</installer-gui-script>
XMLEND

# build installation bundle

pushd ${TMPDIR}
productbuild --distribution "distribution.xml" --package-path "." --resources ${SOURCEDIR}/scripts/installer_mac/Resources "$OUTPUT_BASE_FILENAME.pkg"
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

# clean up

#rm distribution.xml
#rm Surge_*.pkg
