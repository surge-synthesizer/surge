#!/bin/bash

INDIR=$1
SOURCEDIR=$2
TARGET_DIR=$3
VERSION=$4


# Valid version for DEB start with a digit.
DEB_VERSION=$VERSION
if [[ ${DEB_VERSION:0:1} =~ [0-9] ]]; then
    echo "DEB VERSION same as version (${DEB_VERSION})"
elif [[ ${DEB_VERSION} = NIGHTLY* ]]; then
    DEB_VERSION="9.${VERSION}"
    echo "NIGHTLY; DEB VERSION is ${DEB_VERSION}"
else
    DEB_VERSION="0.${VERSION}"
    echo "DEB VERSION is ${DEB_VERSION}"
fi

mkdir ./installer-tmp
pushd ./installer-tmp

# Names
SURGE_NAME=surge-xt
PACKAGE_NAME="$SURGE_NAME"

# locations

# Cleanup from failed prior runs
rm -rf ${PACKAGE_NAME} product
mkdir -p ${PACKAGE_NAME}/usr/lib/vst3
mkdir -p ${PACKAGE_NAME}/usr/lib/lv2
mkdir -p ${PACKAGE_NAME}/usr/lib/clap
mkdir -p ${PACKAGE_NAME}/usr/bin
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
Depends: libc6 (>=2.27), libcairo2, libfontconfig1, libfreetype6, libx11-6, libxcb-cursor0, libxcb-xkb1, libxcb1, libxkbcommon-x11-0, libxkbcommon0,  xdg-utils, xclip
Provides: vst-plugin
Section: sound
Priority: optional
Description: Subtractive hybrid synthesizer virtual instrument
 Surge XT includes VST3 and CLAP instrument formats for use in compatible hosts and a standalone executable
EOT

touch ${PACKAGE_NAME}/usr/share/${SURGE_NAME}/doc/changelog.Debian
DATE=`date --rfc-email`
MSG=`git log -n 1 --pretty="%s (git hash %H)"`
cat <<EOT > ${PACKAGE_NAME}/usr/share/${SURGE_NAME}/doc/changelog.Debian
${PACKAGE_NAME} (${DEB_VERSION}) stable; urgency=medium

  * ${MSG}
  * For more details, see: https://github.com/surge-synthesizer/surge

 -- Surge Synthesizer Team <noreply@github.com>  ${DATE}
EOT
gzip -9 -n ${PACKAGE_NAME}/usr/share/${SURGE_NAME}/doc/changelog.Debian

#copy data and vst plugins

cp ${SOURCEDIR}/LICENSE ${PACKAGE_NAME}/usr/share/${SURGE_NAME}/doc/copyright
cp -r ${SOURCEDIR}/resources/data/* ${PACKAGE_NAME}/usr/share/${SURGE_NAME}/
cp -r ${SOURCEDIR}/scripts/installer_linux/assets/* ${PACKAGE_NAME}/usr/share

# Copy the VST3 bundle
cp -r "${INDIR}/Surge XT.vst3" ${PACKAGE_NAME}/usr/lib/vst3/
cp -r "${INDIR}/Surge XT Effects.vst3" ${PACKAGE_NAME}/usr/lib/vst3/
cp -r "${INDIR}/Surge XT" ${PACKAGE_NAME}/usr/bin/
cp -r "${INDIR}/surge-xt-cli" ${PACKAGE_NAME}/usr/bin/
cp -r "${INDIR}/Surge XT Effects" ${PACKAGE_NAME}/usr/bin/

# set permissions on shared libraries
find ${PACKAGE_NAME}/usr/lib/vst3/ -type f -iname "*.so" -exec chmod 0644 {} +

if [[ -f "${INDIR}/Surge XT.clap" ]]; then
  echo "Installing CLAP components"
  cp -r "${INDIR}/Surge XT.clap" ${PACKAGE_NAME}/usr/lib/clap/
  cp -r "${INDIR}/Surge XT Effects.clap" ${PACKAGE_NAME}/usr/lib/clap/
  find ${PACKAGE_NAME}/usr/lib/clap/ -type f -iname "*.so" -exec chmod 0644 {} +
fi

if [[ -d "${INDIR}/Surge XT.lv2" ]]; then
  cp -r "${INDIR}/Surge XT.lv2" ${PACKAGE_NAME}/usr/lib/lv2/
  cp -r "${INDIR}/Surge XT Effects.lv2" ${PACKAGE_NAME}/usr/lib/lv2/
  find ${PACKAGE_NAME}/usr/lib/lv2/ -type f -iname "*.so" -exec chmod 0644 {} +
fi

echo "----- LIBRARY CONTENTS (except resource) -----"
find ${PACKAGE_NAME}/usr/lib -print
find ${PACKAGE_NAME}/usr/bin -print

# build deb package
dpkg-deb --verbose --build ${PACKAGE_NAME} ${TARGET_DIR}/${PACKAGE_NAME}-linux-x64-${VERSION}.deb

# create a tarball of the {PACKAGE_NAME}/usr contents.
pigz="$(command -v pigz)"

pushd ${PACKAGE_NAME}/usr
    tar cf - ./* | "${pigz:-gzip}" > "${TARGET_DIR}/${PACKAGE_NAME}-linux-x86_64-${VERSION}.tar.gz"
popd


rm -rf ${PACKAGE_NAME}
popd

