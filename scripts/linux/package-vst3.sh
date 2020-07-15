#!/bin/sh

OUTPUT_DIR="$2"
ARCH=`uname -m`

BUNDLE_NAME="Surge.vst3"
BUNDLE_DIR="$OUTPUT_DIR/$BUNDLE_NAME"

echo "Creating Linux VST3 Bundle form ${ARCH}..."

# create basic bundle structure

if test -d "$BUNDLE_DIR"; then
	rm -rf "$BUNDLE_DIR"
fi

VST_SO_DIR="$BUNDLE_DIR/Contents/${ARCH}-linux"
mkdir -p "$VST_SO_DIR"
cp $1 "$VST_SO_DIR"/Surge.so
strip -s "$VST_SO_DIR"/Surge.so
