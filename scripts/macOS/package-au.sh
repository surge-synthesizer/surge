#!/bin/bash          

# If no vectors are specified, use the original-vector by default
if [[ -z "$SURGE_USE_VECTOR_SKIN" ]]; then
    SURGE_USE_VECTOR_SKIN=original-vector
fi

# input config
RES_SRC_LOCATION="resources"
PACKAGE_SRC_LOCATION="$RES_SRC_LOCATION/osx-au"

FONT_SRC_LOCATION="$RES_SRC_LOCATION/fonts"
SVG_SRC_LOCATION="assets/${SURGE_USE_VECTOR_SKIN}/SVG/exported"

BUNDLE_RES_SRC_LOCATION="$RES_SRC_LOCATION/osx-resources"
EXEC_LOCATION="target/au/Release/Surge++.dylib"
#EXEC_LOCATION="target/au/Debug/Surge-Debug.dylib"

# output configs
OUTPUT_DIR=products
BUNDLE_NAME="Surge++.component"
BUNDLE_DIR="$OUTPUT_DIR/$BUNDLE_NAME"
EXEC_TARGET_NAME="Surge++"

echo "Creating AudioUnit (AU) Bundle..."

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

mkdir "$BUNDLE_DIR/Contents/Resources/svg"
cp $SVG_SRC_LOCATION/*svg "$BUNDLE_DIR/Contents/Resources/svg"

mkdir "$BUNDLE_DIR/Contents/Resources/fonts";
cp $FONT_SRC_LOCATION/* "$BUNDLE_DIR/Contents/Resources/fonts";
