#!/bin/sh
#
#    Audionaut - Audio editing application for multitrack recordings.
#    Copyright (C) 2025 Klaus Voltmer
#
#    Audionaut uses a GPL/commercial licence - see LICENCE.md for details.
#
# Builds the macOS disk image for the GitHub release. The result is
# Audionaut-<version>-arm64.dmg in this directory: the Release app signed
# with the hardened runtime and the entitlements next to this script, plus
# an Applications shortcut for drag-to-install. Notarization is not done
# here - the release workflow submits the finished image (it needs the App
# Store Connect credentials); a locally built image can be notarized with
# `xcrun notarytool submit <dmg> --wait` and `xcrun stapler staple <dmg>`.
#
# The app is compiled with AUDIONAUT_GITHUB_CHANNEL so its update check
# asks GitHub Releases rather than the App Store; the Xcode/App Store build
# is untouched.
#
# Usage: package.sh [--no-build]
#   --no-build   package the app already sitting in
#                ../../Builds/MacOSX/build/Release instead of compiling one.
#
# Environment:
#   SIGN_IDENTITY  codesign identity, e.g. "Developer ID Application: ..."
#                  Defaults to "-" (ad-hoc), which builds an image that runs
#                  locally but that Gatekeeper rejects on other machines.
#
# Requires: Xcode command line tools; the Essentia static libraries built
# with ../../Builds/build_essentia.sh.

set -eu

BUILD=1
for arg in "$@"; do
    case "$arg" in
        --no-build) BUILD=0 ;;
        *) echo "package.sh: unknown option '$arg'" >&2; exit 1 ;;
    esac
done

cd "$(dirname "$0")"

XCODE_DIR=../../Builds/MacOSX
APP="$XCODE_DIR/build/Release/Audionaut.app"
SIGN_IDENTITY="${SIGN_IDENTITY:--}"
STAGING=staging

# keep the image name in step with the project version in the .jucer.
# requiring three components skips the version attribute on the XML
# declaration; same idiom as the tag check in the release workflows.
VERSION=$(grep -o 'version="[0-9]\+\.[0-9]\+\.[0-9]\+"' ../../Audionaut.jucer | head -1 | cut -d'"' -f2)
if [ -z "$VERSION" ]; then
    echo "package.sh: could not read the project version from Audionaut.jucer" >&2
    exit 1
fi
DMG="Audionaut-$VERSION-arm64.dmg"
echo "Packaging Audionaut $VERSION as $DMG (signing as: $SIGN_IDENTITY)"

if [ "$BUILD" -eq 1 ]; then
    # Ad-hoc signing during the build keeps the Projucer's "Sign Target"
    # script phase working without a certificate in the keychain; the real
    # signature is applied below, because that phase drops the hardened
    # runtime flag anyway. $(inherited) keeps the Projucer's defines.
    xcodebuild -project "$XCODE_DIR/Audionaut.xcodeproj" \
        -scheme "Audionaut - App" \
        -configuration Release \
        -destination 'platform=macOS' \
        CODE_SIGN_IDENTITY=- \
        CODE_SIGN_STYLE=Manual \
        DEVELOPMENT_TEAM= \
        GCC_PREPROCESSOR_DEFINITIONS='$(inherited) AUDIONAUT_GITHUB_CHANNEL=1' \
        build | grep -E '^(\*\*|error|warning: (ld|linker)|Signing|Ld |CodeSign)' || true
fi
if [ ! -d "$APP" ]; then
    echo "package.sh: $APP does not exist" >&2
    exit 1
fi

# The bundle has no nested code (JUCE, Essentia, Rubber Band and demucs are
# all linked statically), so signing the bundle itself is the whole job.
echo "Signing $APP"
codesign --force --options runtime --timestamp \
    --entitlements Audionaut.entitlements \
    --sign "$SIGN_IDENTITY" "$APP"
codesign --verify --strict --verbose=2 "$APP"
codesign --display --verbose=2 "$APP" 2>&1 | grep -E '^(Identifier|Authority|TeamIdentifier|CodeDirectory|Timestamp)'

rm -rf "$STAGING" "$DMG"
mkdir -p "$STAGING"
cp -R "$APP" "$STAGING/"
ln -s /Applications "$STAGING/Applications"

hdiutil create -volname "Audionaut $VERSION" -srcfolder "$STAGING" -format UDZO -ov -quiet "$DMG"
rm -rf "$STAGING"

codesign --force --timestamp --sign "$SIGN_IDENTITY" "$DMG"
codesign --verify --verbose=2 "$DMG"

echo "Built $DMG"
