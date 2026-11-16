#!/bin/sh
#
#    Audionaut - Audio editing application for multitrack recordings.
#    Copyright (C) 2025 Klaus Voltmer
#
#    Audionaut uses a GPL/commercial licence - see LICENCE.md for details.
#
# The Xcode project's post-build step (set in Audionaut.jucer, "Post-Build
# Shell Script"): builds audionaut-scan-helper with CMake, puts it into the
# app bundle at Contents/Helpers and signs it with the two entitlements a
# child of the sandboxed app may have (ScanHelper.entitlements). It runs
# before the bundle itself is signed, so the bundle's signature covers the
# helper.
#
# Reads Xcode's build settings from the environment: CONFIGURATION, ARCHS,
# MACOSX_DEPLOYMENT_TARGET, PROJECT_TEMP_DIR, TARGET_BUILD_DIR,
# CONTENTS_FOLDER_PATH, PRODUCT_BUNDLE_IDENTIFIER, EXPANDED_CODE_SIGN_IDENTITY
# / CODE_SIGN_IDENTITY, ENABLE_HARDENED_RUNTIME.
#
# Requires: CMake 3.15 or newer (brew install cmake).

set -eu

HERE=$(cd "$(dirname "$0")" && pwd)

# Xcode runs scripts with a bare PATH
PATH="$PATH:/opt/homebrew/bin:/usr/local/bin:/Applications/CMake.app/Contents/bin"
if ! command -v cmake >/dev/null 2>&1; then
    echo "error: embed-scan-helper.sh: cmake not found - the plugin scan helper is built with it (brew install cmake)" >&2
    exit 1
fi

: "${TARGET_BUILD_DIR:?embed-scan-helper.sh runs as an Xcode build step}"
CONFIGURATION="${CONFIGURATION:-Release}"
case "$CONFIGURATION" in
    Debug) BUILD_TYPE=Debug ;;
    *)     BUILD_TYPE=Release ;;
esac

# one build tree per configuration, next to Xcode's own intermediates
BUILD_DIR="${PROJECT_TEMP_DIR:-$TARGET_BUILD_DIR}/scan-helper-$CONFIGURATION"
ARCHITECTURES=$(echo "${ARCHS:-$(uname -m)}" | tr ' ' ';')

# Of Xcode's settings in the environment, the SDK and the deployment target
# are passed on explicitly; nothing else should steer the compiler.
unset SDKROOT CC CXX LD CFLAGS CXXFLAGS LDFLAGS

cmake -S "$HERE" -B "$BUILD_DIR" \
    -DCMAKE_BUILD_TYPE="$BUILD_TYPE" \
    -DCMAKE_OSX_ARCHITECTURES="$ARCHITECTURES" \
    -DCMAKE_OSX_DEPLOYMENT_TARGET="${MACOSX_DEPLOYMENT_TARGET:-13.0}" \
    > "$BUILD_DIR.configure.log" 2>&1 || { cat "$BUILD_DIR.configure.log" >&2; exit 1; }
cmake --build "$BUILD_DIR" --target AudionautScanHelper --parallel

HELPER="$BUILD_DIR/AudionautScanHelper_artefacts/$BUILD_TYPE/audionaut-scan-helper"
DESTINATION="$TARGET_BUILD_DIR/${CONTENTS_FOLDER_PATH:?}/Helpers"
mkdir -p "$DESTINATION"
cp -f "$HELPER" "$DESTINATION/audionaut-scan-helper"

IDENTITY="${EXPANDED_CODE_SIGN_IDENTITY:-${CODE_SIGN_IDENTITY:--}}"
[ -n "$IDENTITY" ] || IDENTITY=-

set -- --force --sign "$IDENTITY" \
    --identifier "${PRODUCT_BUNDLE_IDENTIFIER:-com.voltmer-systems.audionaut}.scan-helper" \
    --entitlements "$HERE/ScanHelper.entitlements" --generate-entitlement-der
if [ "${ENABLE_HARDENED_RUNTIME:-NO}" = "YES" ]; then
    set -- "$@" --options runtime
fi
if [ "$IDENTITY" != "-" ] && [ "$BUILD_TYPE" = "Release" ]; then
    set -- "$@" --timestamp
fi

echo "Signing $DESTINATION/audionaut-scan-helper as: $IDENTITY"
codesign "$@" "$DESTINATION/audionaut-scan-helper"
