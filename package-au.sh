#!/bin/bash          

# input config
RES_SRC_LOCATION="resources"
PACKAGE_SRC_LOCATION="$RES_SRC_LOCATION/osx-au"
BITMAP_SRC_LOCATION="$RES_SRC_LOCATION/bitmaps"
BUNDLE_RES_SRC_LOCATION="$RES_SRC_LOCATION/osx-resources"
EXEC_LOCATION="target/au/Release/Surge.dylib"
#EXEC_LOCATION="target/au/Debug/Surge-Debug.dylib"

# output configs
OUTPUT_DIR=products
BUNDLE_NAME="Surge.component"
BUNDLE_DIR="$OUTPUT_DIR/$BUNDLE_NAME"
EXEC_TARGET_NAME="Surge"

echo Creating AudioUnit (AU) Bundle...

# create basic bundle structure

if test -d "$BUNDLE_DIR"; then
	rm -rf "$BUNDLE_DIR"
fi

mkdir -p "$BUNDLE_DIR/Contents/MacOS"

# copy executable
cp "$EXEC_LOCATION" "$BUNDLE_DIR/Contents/MacOS/$EXEC_TARGET_NAME"

# copy Info.plist and PkgInfo
cp $PACKAGE_SRC_LOCATION/* "$BUNDLE_DIR/Contents/"

# copy bundle resources
cp -R "$BUNDLE_RES_SRC_LOCATION" "$BUNDLE_DIR/Contents/Resources"
cp $BITMAP_SRC_LOCATION/* "$BUNDLE_DIR/Contents/Resources/"
mkdir -f
cp -rf resources/data "$BUNDLE_DIR/Contents/Data"
