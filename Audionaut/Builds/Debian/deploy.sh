#!/bin/sh
set -e

# Builds the Debian package for Audionaut. The result is
# audionaut_<version>_amd64.deb in this directory, containing the stripped
# release binary plus the desktop entry, icon and MIME type that give it a
# menu entry and let .audium projects be opened from a file manager.
#
# Usage: deploy.sh [--no-build]
#   --no-build   package the binary already sitting in ../LinuxMakefile/build
#                instead of compiling one. Used by the release workflow, which
#                compiles in its own step.
#
# Honours JOBS to override the compile parallelism. The default stays at 8:
# this tree needs well over a GB of RAM per compile job, so scaling to the core
# count exhausts memory on a typical workstation.
#
# Requires: equivs (apt install equivs)

BUILD=1
for arg in "$@"; do
    case "$arg" in
        --no-build) BUILD=0 ;;
        *) echo "deploy.sh: unknown option '$arg'" >&2; exit 1 ;;
    esac
done

cd "$(dirname "$0")"

MAKEFILE_DIR=../LinuxMakefile
RESOURCES_DIR=../../Resources
STAGING=staging

# keep the package version in step with the project version in the .jucer.
# requiring three components skips the version attribute on the XML
# declaration; same idiom as the tag check in release-linux.yml.
VERSION=$(grep -o 'version="[0-9]\+\.[0-9]\+\.[0-9]\+"' ../../Audionaut.jucer | head -1 | cut -d'"' -f2)
if [ -z "$VERSION" ]; then
    echo "deploy.sh: could not read the project version from Audionaut.jucer" >&2
    exit 1
fi
echo "Packaging audionaut $VERSION"

rm -rf "$STAGING"

if [ "$BUILD" -eq 1 ]; then
    # clean
    rm -rf "$MAKEFILE_DIR/build"

    # build release
    make CONFIG=Release -j"${JOBS:-8}" -C "$MAKEFILE_DIR"
    make strip -C "$MAKEFILE_DIR"
elif [ ! -f "$MAKEFILE_DIR/build/audionaut" ]; then
    echo "deploy.sh: --no-build given but $MAKEFILE_DIR/build/audionaut does not exist" >&2
    exit 1
fi

# stage the payload under the names it is installed with -> lower case "audionaut"
mkdir -p "$STAGING"
cp "$MAKEFILE_DIR/build/audionaut" "$STAGING/audionaut"
cp "$RESOURCES_DIR/audionaut.desktop" "$STAGING/audionaut.desktop"
cp audionaut.xml "$STAGING/audionaut.xml"
cp "$RESOURCES_DIR/audionaut_logo_512x512.png" "$STAGING/audionaut.png"

sed "s/@VERSION@/$VERSION/" audionaut-equvis.conf > "$STAGING/audionaut.conf"

# catch typos in the metadata before they ship
if command -v desktop-file-validate > /dev/null; then
    desktop-file-validate "$STAGING/audionaut.desktop"
fi
if command -v xmllint > /dev/null; then
    xmllint --noout "$STAGING/audionaut.xml"
fi

# create debian package
equivs-build "$STAGING/audionaut.conf"

echo "Built audionaut_${VERSION}_amd64.deb"
echo "Install with: sudo apt install ./audionaut_${VERSION}_amd64.deb"
