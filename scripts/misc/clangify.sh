#!/bin/sh -e

if ! command -v clang-format; then
    echo "clang-format missing. Please install with brew or equivalent."
    exit 1
fi

FORMAT_FILE="./.clang-format"
if ! [ -f "$FORMAT_FILE" ] ; then
    echo "$FORMAT_FILE style definition file missing"
    exit 1
fi

EXTENSIONS="c cpp cxx h mm"
echo "Starting clang-format of the entire repository for file extensions: $EXTENSIONS"

for EXTENSION in $EXTENSIONS ; do
    echo "Formatting files with extension: $EXTENSION"
    SELECTOR="*.$EXTENSION"
    find src -name "$SELECTOR" -exec clang-format -style=file -i -verbose "{}" \;
    find libs/escape-from-vstgui -name "$SELECTOR" -exec clang-format -style=file -i -verbose "{}" \;
    find libs/filesystem -name "$SELECTOR" -exec clang-format -style=file -i -verbose "{}" \;
done

echo "All files successfully formatted"
