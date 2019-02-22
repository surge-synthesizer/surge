#!/bin/sh

OUTPUT_DIR=products
BUNDLE_NAME="Surge.vst3"
BUNDLE_DIR="$OUTPUT_DIR/$BUNDLE_NAME"

echo "Creating Linux VST3 Bundle..."

# create basic bundle structure

if test -d "$BUNDLE_DIR"; then
	rm -rf "$BUNDLE_DIR"
fi

VST_SO_DIR="$BUNDLE_DIR/Contents/x86_64-linux"
mkdir -p "$VST_SO_DIR"
cp target/vst3/Release/Surge.so "$VST_SO_DIR"
