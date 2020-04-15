#!/bin/bash

# preflight check
if [[ ! -f ./make_deb.sh ]]; then
	echo "You must run this script from within the installer_linux directory!"
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

# Valid version for DEB start with a digit.
DEB_VERSION=$VERSION
if [[ ${DEB_VERSION:0:1} =~ [0-9] ]]; then
    echo "DEB VERSION same as Version"
else
    DEB_VERSION="0.${VERSION}"
    echo "DEB VERSION is ${DEB_VERSION}"
fi


# Names
SURGE_NAME=surge
PACKAGE_NAME="$SURGE_NAME"
# SURGE_NAME=surge-synthesizer

# locations

# Cleanup from failed prior runs
rm -rf ${PACKAGE_NAME} product
mkdir -p ${PACKAGE_NAME}/usr/lib/vst
mkdir -p ${PACKAGE_NAME}/usr/lib/vst3
mkdir -p ${PACKAGE_NAME}/usr/lib/lv2
mkdir -p ${PACKAGE_NAME}/usr/share/${SURGE_NAME}/doc
mkdir -p ${PACKAGE_NAME}/DEBIAN
chmod -R 0755 ${PACKAGE_NAME}

# build control file

if [[ -f ${PACKAGE_NAME}/DEBIAN/control ]]; then
	rm ${PACKAGE_NAME}/DEBIAN/control
fi
touch ${PACKAGE_NAME}/DEBIAN/control
cat <<EOT >> ${PACKAGE_NAME}/DEBIAN/control
Source: ${PACKAGE_NAME}
Package: ${PACKAGE_NAME}
Version: $DEB_VERSION
Architecture: amd64
Maintainer: surgeteam <noreply@github.com>
Depends: libcairo2, libfontconfig1, libfreetype6, libx11-6, libxcb-cursor0, libxcb-util1, libxcb-xkb1, libxcb1, libxkbcommon-x11-0, libxkbcommon0, fonts-lato, xdg-utils, zenity
Provides: vst-plugin
Section: sound
Priority: optional
Description: Subtractive hybrid synthesizer virtual instrument
 Surge includes VST2, VST3, and LV2 virtual instrument formats for use in compatible hosts.
EOT

touch ${PACKAGE_NAME}/usr/share/${SURGE_NAME}/doc/changelog.Debian
DATE=`date --rfc-email`
MSG=`git log -n 1 --pretty="%s (git hash %H)"`
cat <<EOT > ${PACKAGE_NAME}/usr/share/${SURGE_NAME}/doc/changelog.Debian
${PACKAGE_NAME} (${DEB_VERSION}) stable; urgency=medium

  * ${MSG}
  * For more details see https://github.com/surge-synthesizer/surge

 -- Surge Synthesizer Team <noreply@github.com>  ${DATE}
EOT
gzip -9 -n ${PACKAGE_NAME}/usr/share/${SURGE_NAME}/doc/changelog.Debian

#copy data and vst plugins

cp ../LICENSE ${PACKAGE_NAME}/usr/share/${SURGE_NAME}/doc/copyright
cp -r ../resources/data/* ${PACKAGE_NAME}/usr/share/${SURGE_NAME}/
cp ../products/Surge.so ${PACKAGE_NAME}/usr/lib/vst/${SURGE_NAME}.so

# Once VST3 works, this will be ../products/vst3
cp -r ../products/Surge.vst3 ${PACKAGE_NAME}/usr/lib/vst3/

# copy the lv2 bundle
cp -r ../products/Surge.lv2 ${PACKAGE_NAME}/usr/lib/lv2/

# set permissions on shared libraries
chmod -R 0644 ${PACKAGE_NAME}/usr/lib/vst/${SURGE_NAME}.so
find ${PACKAGE_NAME}/usr/lib/vst3/ -type f -iname "*.so" | xargs chmod 0644
find ${PACKAGE_NAME}/usr/lib/lv2/ -type f -iname "*.so" | xargs chmod 0644

# build package

mkdir -p product
dpkg-deb --build ${PACKAGE_NAME} product/${PACKAGE_NAME}-linux-x64-${VERSION}.deb
rm -rf ${PACKAGE_NAME}

echo "Built DEB Package"
pwd
ls -l product
