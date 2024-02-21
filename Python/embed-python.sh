#!/bin/bash

set -e

# Run pyinstaller
pyinstaller --noconfirm hello.py


if [ -n "$CONFIGURATION"]; then
    CONFIGURATION="Debug"
fi

echo "config: $CONFIGURATION"

DIST_FOLDER="dist/hello"
CONTENTS_FOLDER="../AppAudium/Builds/MacOSX/build/"$CONFIGURATION"/Audionatomist.app/Contents"

# Copy output to application's contents folder
echo "cp -r "$DIST_FOLDER" "$CONTENTS_FOLDER""
cp -r "$DIST_FOLDER" "$CONTENTS_FOLDER"

#--preserve=links
