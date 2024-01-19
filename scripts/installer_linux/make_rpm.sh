#!/bin/bash
#
# create a Surge XT RPM package.
# see the RPM Packaging guide at https://rpm-packaging-guide.github.io/


# check to ensure rpm build tool exists
if ! [[ -x "$(command -v rpmbuild)" ]]
then
    echo "rpmbuild not found! Please install rpm build tools!"
    exit -1
fi

# parameter check
if [[ "$#" -ne 4 ]]; then
    echo "ERROR: make_rpm.sh requires 4 parameters."
    echo
    echo "    make_rpm.sh INDIR SOURCEDIR TARGET_DIR VERSION"
    echo
    echo "    INDIR       Location of intermediate build artifacts"
    echo "    SOURCEDIR   Location of source code/license file"
    echo "    TARGET_DIR  Where to put the output"
    echo "    VERSION     build/rpm version"
    echo
    exit -1
fi

# Intermediate build artifacts
INDIR=$1
# license file location
SOURCEDIR=$2
# where to put the output
TARGET_DIR=$3
# Build version
VERSION=$4

echo running make_rpm.sh $INDIR $SOURCEDIR $TARGET_DIR $VERSION

RPM_VERSION=$VERSION
# generate/modify version
if [[ ${VERSION:0:1} =~ [0-9] ]]; then
    echo "Build Version (${RPM_VERSION})"
elif [[ ${VERSION} = NIGHTLY* ]]; then
    # rpmbuild does not allow dashes in the rpm version.
    RPM_VERSION="9.${VERSION//-/$'.'}"
    echo "NIGHTLY; VERSION is ${VERSION}"
else
    RPM_VERSION="0.${VERSION}"
    echo "VERSION is ${VERSION}"
fi

# TODO Do we want to parameterize this?
RPM_RELEASE=1
ARCH=$(uname -m)

# create and enter temporary build package directory.
mkdir -p ./installer-tmp
pushd ./installer-tmp

# Name of Package
PACKAGE_NAME=surge-xt

# Date in RFC format for the Changelog
DATE_RFC=`date --rfc-email`
# Date in Day-of-Week Month Day Year format for the rpm config header.
DATE_RPM=`date +"%a %b %d %Y"`
MSG=`git log -n 1 --pretty="%s (git hash %H)"`

# generate changelog to put in install package
touch changelog.txt
cat <<EOT > changelog.txt
${PACKAGE_NAME} (${VERSION}) stable; urgency=medium

   * ${MSG}
   * For more details, see: https://github.com/surge-synthesizer/surge

  -- Surge Synthesizer Team <noreply@github.com>  ${DATE_RFC}
EOT


# build rpm spec file
RPM_SPEC_FILE=${PACKAGE_NAME}.spec

touch ${RPM_SPEC_FILE}
cat << EOT > ${RPM_SPEC_FILE}
Name:       ${PACKAGE_NAME}
Version:    ${RPM_VERSION}
Release:    ${RPM_RELEASE}
BuildArch:  ${ARCH}
Summary:    Subtractive hybrid synthesizer virtual instrument
License:    GPLv3+
URL: https://surge-synthesizer.github.io/
Source: https://github.com/surge-synthesizer/surge
Provides: vst-plugin
Group: sound


%description
Subtractive hybrid synthesizer virtual instrument
Surge XT includes VST3 and CLAP instrument formats for use in compatible hosts and a standalone executable

%install
install -m 0755 -d %{buildroot}%{_bindir}/
install -m 0755 -d %{buildroot}%{_datadir}/
install -m 0755 -d %{buildroot}%{_defaultdocdir}/
install -m 0755 -d %{buildroot}%{_libdir}/vst3/
install -m 0755 -d %{buildroot}%{_libdir}/clap/
install -m 0755 -d %{buildroot}%{_libdir}/lv2/
install -m 0755 -d %{buildroot}%{_datarootdir}
install -m 0755 -d %{buildroot}%{_datarootdir}/surge-xt/
install -m 0755 -d %{buildroot}%{_datarootdir}/surge-xt/doc
install -m 0644 ${SOURCEDIR}/LICENSE %{buildroot}%{_datarootdir}/surge-xt/doc/copyright
install -m 0644 $(pwd)/changelog.txt %{buildroot}%{_datarootdir}/surge-xt/doc/changelog.txt

cp -r ${SOURCEDIR}/resources/data/* %{buildroot}%{_datarootdir}/surge-xt/
cp -r "${INDIR}/Surge XT.vst3" %{buildroot}%{_libdir}/vst3/
cp -r "${INDIR}/Surge XT Effects.vst3" %{buildroot}%{_libdir}/vst3/
cp -r "${INDIR}/Surge XT.clap" %{buildroot}%{_libdir}/clap/
cp -r "${INDIR}/Surge XT Effects.clap" %{buildroot}%{_libdir}/clap/
cp -r "${INDIR}/Surge XT.lv2" %{buildroot}%{_libdir}/lv2/
cp -r "${INDIR}/Surge XT Effects.lv2" %{buildroot}%{_libdir}/lv2/

# I have no idea why the docker image makes these
rm -rf "%{buildroot}%{_libdir}/.build-id"
rm -rf "%{buildroot}lib/.build-id"

# install executable files as executable
install -m 0755 "${INDIR}/Surge XT" %{buildroot}/%{_bindir}
install -m 0755 "${INDIR}/surge-xt-cli" %{buildroot}/%{_bindir}
install -m 0755 "${INDIR}/Surge XT Effects" %{buildroot}/%{_bindir}

# set permissions on shared libraries
find %{buildroot}%{_libdir}/vst3/ -type f -iname "*.so" -exec chmod 0644 {} +


%files
"%{_bindir}/Surge XT"
"%{_bindir}/surge-xt-cli"
"%{_bindir}/Surge XT Effects"
"%{_libdir}/vst3/Surge XT.vst3"
"%{_libdir}/vst3/Surge XT Effects.vst3"
"%{_libdir}/clap/Surge XT.clap"
"%{_libdir}/clap/Surge XT Effects.clap"
"%{_libdir}/lv2/Surge XT.lv2"
"%{_libdir}/lv2/Surge XT Effects.lv2"
"%{_datarootdir}/surge-xt/*"


%changelog
* ${DATE_RPM} Surge Synthesizer Team <noreply@github.com>  - ${VERSION}
- Surge XT package
- ${MSG} ${DATE_RFC}
- For more details, see: https://github.com/surge-synthesizer/surge

EOT

# create the folders to build the rpm
RPM_BUILD_DIR=$(pwd)/rpmbuild
mkdir -p $RPM_BUILD_DIR/{BUILD,BUILDROOT,RPMS,SOURCES,SPECS,SRPMS}

# build rpm package
rpmbuild -bb --define "_topdir $RPM_BUILD_DIR" ${RPM_SPEC_FILE}

# TODO add key signing, or sign as a post build step

# move rpm to target directory
mv $RPM_BUILD_DIR/RPMS/$ARCH/$PACKAGE_NAME-${RPM_VERSION}-$RPM_RELEASE.$ARCH.rpm ${TARGET_DIR}/${PACKAGE_NAME}-$ARCH-${VERSION}.rpm

if [[ $? -ne 0 ]]
then
    echo Error moving rpm file, rpmbuild failed!
    popd
    exit -1
fi

popd
