#!/bin/bash
# This script just creates a dummy install path. It doesn't actually work per se (the notarization
# should fail) because I don't create a valid package, but this will let me very rapidly test
# the pipeline (and then delete this)


TMPDIR="./installer-dummy"
mkdir -p $TMPDIR



OUTPUT_BASE_FILENAME="surge-dummy"

cat << EOF > ${TMPDIR}/main.cpp
#include <iostream>
int main(int argc, char **argv) { std::cout << "Hello World" << std::endl; }
EOF

clang++ -std=c++17 -o $TMPDIR/main $TMPDIR/main.cpp
codesign --force -s "$MAC_SIGNING_CERT" -o runtime --deep "${TMPDIR}/main"
codesign -vvv "${TMPDIR}/main"

pkgbuild --sign "$MAC_INSTALLING_CERT" --root $TMPDIR --identifier "org.surge-synth-team.dummy.pkg" --version 0.9.0 --install-location "/tmp" "$TMPDIR/sxt-dummy.pkg"

mkdir ${TMPDIR}/vol
mv ${TMPDIR}/sxt-dummy.pkg ${TMPDIR}/vol

hdiutil create /tmp/tmp.dmg -ov -volname "DUMMY" -fs HFS+ -srcfolder "${TMPDIR}/vol"
hdiutil convert /tmp/tmp.dmg -format UDZO -o "${TMPDIR}/Dummy.dmg"

codesign --force -s "$MAC_SIGNING_CERT" --timestamp "${TMPDIR}/Dummy.dmg"
codesign -vvv "${TMPDIR}/Dummy.dmg"
xcrun notarytool submit "${TMPDIR}/Dummy.dmg" --apple-id ${MAC_SIGNING_ID} --team-id ${MAC_SIGNING_TEAM} --password ${MAC_SIGNING_1UPW} --wait
xcrun stapler staple "${TMPDIR}/Dummy.dmg"
exit 0
