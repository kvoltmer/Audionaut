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

# Essentia's bundled waf needs the stdlib "distutils" module, which was removed
# in Python 3.12. Pick an interpreter that still provides it (honour $PYTHON
# first, then try common names). Override with PYTHON=... if none is found.
pick_python() {
    for candidate in "${PYTHON:-}" python3 python3.11 python3.10 python3.9 python; do
        [ -n "$candidate" ] || continue
        if command -v "$candidate" >/dev/null 2>&1 \
            && "$candidate" -c 'import distutils' >/dev/null 2>&1; then
            echo "$candidate"; return 0
        fi
    done
    return 1
}

if ! PYTHON="$(pick_python)"; then
    echo "error: no python with the 'distutils' module found (needed by waf)." >&2
    echo "       install one (e.g. 'brew install python@3.11') or set PYTHON=..." >&2
    exit 1
fi
echo "→ Using python: $PYTHON ($("$PYTHON" --version 2>&1))"

if [ ! -x "$ESSENTIA_DIR/waf" ]; then
    echo "error: waf not found at $ESSENTIA_DIR/waf - is the essentia submodule checked out?" >&2
    exit 1
fi

cd "$ESSENTIA_DIR"

# 0. Patch Essentia's (x86/Linux-oriented) 3rd-party build scripts so they build
#    on modern macOS / Apple Silicon. All edits are idempotent, so re-runs and
#    already-patched checkouts are no-ops - we deliberately patch the working
#    tree here instead of committing into the essentia submodule.
apply_essentia_build_patches() {
    local cfg="packaging/build_config.sh"
    local taglib="packaging/debian_3rdparty/build_taglib.sh"

    # zlib 1.2.12's zutil.h stubs fdopen() under TARGET_OS_MAC (classic Mac OS),
    # which modern macOS SDKs define =1 - breaking <stdio.h>. Use a newer zlib.
    if grep -q '^ZLIB_VERSION=zlib-1\.2\.12$' "$cfg"; then
        echo "  · zlib 1.2.12 -> 1.3.1 (modern-macOS build fix)"
        sed -i.bak 's/^ZLIB_VERSION=zlib-1\.2\.12$/ZLIB_VERSION=zlib-1.3.1/' "$cfg"
        rm -f "$cfg.bak"
    fi

    # fftw-3.3.2 only knows 32-bit ARM NEON (-mfpu=neon), which fails on arm64
    # where NEON is baseline. Drop the x86 SSE2 flags there and build generic.
    case "$(uname -m)" in
        arm64|aarch64)
            if grep -q '^[[:space:]]*--enable-sse2$' "$cfg"; then
                echo "  · fftw: drop x86 SSE2 flags on arm64 (build generic)"
                sed -i.bak \
                    -e '/^[[:space:]]*--enable-sse2$/d' \
                    -e '/^[[:space:]]*--with-incoming-stack-boundary=2$/d' \
                    -e '/^[[:space:]]*--with-our-malloc16$/d' "$cfg"
                rm -f "$cfg.bak"
            fi
            ;;
    esac

    # macOS/BSD sed needs a backup suffix for -i; the -i.bak form works on both
    # GNU and BSD sed.
    if grep -qF "sed -i 's/-ltag/-ltag -lz/g' taglib.pc" "$taglib"; then
        echo "  · taglib: portable in-place sed (BSD/macOS)"
        sed -i.bak "s|^sed -i 's/-ltag/-ltag -lz/g' taglib.pc\$|sed -i.bak 's/-ltag/-ltag -lz/g' taglib.pc \&\& rm -f taglib.pc.bak|" "$taglib"
        rm -f "$taglib.bak"
    fi
}

echo "→ Applying macOS/arm64 build patches to Essentia 3rd-party scripts"
apply_essentia_build_patches

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
