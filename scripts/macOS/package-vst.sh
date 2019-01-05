#!/bin/bash          

# input config
RES_SRC_LOCATION="resources"
PACKAGE_SRC_LOCATION="$RES_SRC_LOCATION/osx-vst2"
BITMAP_SRC_LOCATION="$RES_SRC_LOCATION/bitmaps"
VECTOR_BITMAP_SRC_LOCATION="assets/classic-vector/exported"
BUNDLE_RES_SRC_LOCATION="$RES_SRC_LOCATION/osx-resources"
EXEC_LOCATION="target/vst2/Release/Surge.dylib"
#EXEC_LOCATION="target/vst2/Debug/Surge-Debug.dylib"

# output configs
OUTPUT_DIR=products
BUNDLE_NAME="Surge.vst"
BUNDLE_DIR="$OUTPUT_DIR/$BUNDLE_NAME"
EXEC_TARGET_NAME="Surge"

echo Creating VST Bundle...

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

if [[ -z "$SURGE_USE_VECTOR_SKIN" ]]; then
    cp $BITMAP_SRC_LOCATION/* "$BUNDLE_DIR/Contents/Resources/"
else
    rm "$BUNDLE_DIR/Contents/Resources/bmp?????.png"
    cp $VECTOR_BITMAP_SRC_LOCATION/bmp?????.png "$BUNDLE_DIR/Contents/Resources/"
fi

rsync -r --delete resources/data/ "$BUNDLE_DIR/Contents/Data/"
