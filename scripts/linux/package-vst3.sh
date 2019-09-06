#!/bin/sh

OUTPUT_DIR=products

if [ $config = "debug_x64" ]; then
    BUNDLE_NAME="Surge++-Debug.vst3"
else
    BUNDLE_NAME="Surge++.vst3"
fi
BUNDLE_DIR="$OUTPUT_DIR/$BUNDLE_NAME"

echo "Creating Linux VST3 Bundle '${BUNDLE_DIR}'"

# create basic bundle structure

if test -d "$BUNDLE_DIR"; then
	rm -rf "$BUNDLE_DIR"
fi

VST_SO_DIR="$BUNDLE_DIR/Contents/x86_64-linux"
mkdir -p "$VST_SO_DIR"
if [ $config = debug_x64 ]; then
    cp target/vst3/Debug/Surge++-Debug.so "$VST_SO_DIR"
else
    cp target/vst3/Release/Surge++.so "$VST_SO_DIR"
fi
