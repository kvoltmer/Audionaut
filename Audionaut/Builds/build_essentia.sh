#!/usr/bin/env bash
#
#    Audionaut - Audio editing application for multitrack recordings.
#    Copyright (C) 2025 Klaus Voltmer
#
#    Audionaut uses a GPL/commercial licence - see LICENCE.md for details.
#
# Configures and builds the Essentia submodule (the audio-analysis backend).
# Run this after a fresh/clean submodule checkout to produce the static library
# and headers that Audionaut's analysis code links against.
#
# Steps:
#   1. Build Essentia's 3rd-party dependencies statically (Eigen headers + fftw,
#      libav/ffmpeg, libsamplerate, yaml, ... into packaging/debian_3rdparty/).
#      This is the slow step and is skipped when it looks already built; force it
#      with FORCE_3RDPARTY=1.
#   2. waf configure --lightweight (links the static 3rd-party deps).
#   3. waf build.
#
# Override the interpreter with PYTHON=... if "python3" is not on PATH.

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
ESSENTIA_DIR="$SCRIPT_DIR/../../Submodules/essentia"
PYTHON="${PYTHON:-python3}"

if [ ! -x "$ESSENTIA_DIR/waf" ]; then
    echo "error: waf not found at $ESSENTIA_DIR/waf - is the essentia submodule checked out?" >&2
    exit 1
fi

cd "$ESSENTIA_DIR"

# 1. 3rd-party static dependencies (Eigen + the libs listed in --lightweight).
EIGEN_MARKER="packaging/debian_3rdparty/include/eigen3/signature_of_eigen3_matrix_library"
if [ -f "$EIGEN_MARKER" ] && [ "${FORCE_3RDPARTY:-0}" != "1" ]; then
    echo "→ 3rd-party deps already built (found $EIGEN_MARKER); skipping (set FORCE_3RDPARTY=1 to rebuild)"
else
    echo "→ Building Essentia 3rd-party static dependencies (this is slow)"
    bash packaging/build_3rdparty_static_debian.sh
fi

# 2. Configure Essentia against those static deps.
echo "→ Configuring Essentia in $ESSENTIA_DIR"
"$PYTHON" waf configure --lightweight=fftw,libav,libsamplerate,yaml --with-static-examples

# 3. Build.
echo "→ Building Essentia"
"$PYTHON" waf

echo "✓ Essentia build complete (see $ESSENTIA_DIR/build)"
