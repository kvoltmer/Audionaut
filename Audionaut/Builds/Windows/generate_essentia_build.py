#!/usr/bin/env python3
#
#    Audionaut - Audio editing application for multitrack recordings.
#    Copyright (C) 2025 Klaus Voltmer
#
#    Audionaut uses a GPL/commercial licence - see LICENCE.md for details.
#
# Generates the files Essentia's waf build would normally produce, so the
# Windows/MSVC CMake build (CMakeLists.txt next to this script) can compile the
# library without waf. waf does not work on Windows - the win32 branch of
# Essentia's wscript is dead code, its whole body sits inside a triple-quoted
# string - and its 3rd-party scripts are bash targeting MinGW, which produces
# archives MSVC cannot link. See Audionaut/Builds/Windows/README.md.
#
# Everything is written into the build directory; the Essentia submodule is
# never modified. Emits:
#   version.h                     - what create_version_h() writes for waf
#   essentia_algorithms_reg.cpp   - the AlgorithmFactory registration table
#   essentia_sources.cmake        - the source lists, mirroring src/wscript
#
# Requires only a stdlib Python 3 (utils/algorithms_info.py imports os, sys,
# glob and io only). Unlike waf itself it does NOT need distutils, so any
# modern interpreter works.

import os
import sys

# Algorithms excluded from this build, mirroring the ALGOIGNORE that
# src/wscript accumulates for the dependency set we provide via vcpkg
# (fftw3 + ffmpeg + libsamplerate + libyaml, matching build_essentia.sh's
# --lightweight=fftw,libav,libsamplerate,yaml).
IGNORED_ALGORITHMS = [
    # FFTW is present, so the KissFFT (K) and Apple Accelerate (A) variants are
    # dropped - src/wscript does the same. Accelerate would not build here at all.
    'FFTK', 'IFFTK', 'FFTKComplex', 'IFFTKComplex',
    'FFTA', 'IFFTA', 'FFTAComplex', 'IFFTAComplex',
    # No TagLib in the dependency set.
    'MetadataReader', 'MusicExtractor', 'FreesoundExtractor',
    # No Gaia2.
    'GaiaTransform', 'MusicExtractorSVM',
    # No Chromaprint.
    'Chromaprinter',
    # No TensorFlow.
    'TensorflowPredict', 'TensorflowPredictMusiCNN', 'TensorflowPredictVGGish',
    'TensorflowPredictTempoCNN', 'TensorflowPredictCREPE', 'PitchCREPE',
    'TempoCNN', 'TensorflowPredictEffnetDiscogs', 'TensorflowPredict2D',
    'TensorflowPredictFSDSINet', 'TensorflowPredictMAEST',
    # Audio *writing* only. These are the sole users of audiocontext.cpp, which
    # does not compile against a current FFmpeg: it reads AVCodec::sample_fmts,
    # removed from the public struct in FFmpeg 7.1 in favour of
    # avcodec_get_supported_config(). Essentia 2.1-beta6 predates that and only
    # ever checks for avcodec >= 55.34.1 (FFmpeg 2.x).
    #
    # Reading is unaffected - AudioLoader and MonoLoader compile against
    # FFmpeg 9 unchanged - and Audionaut writes audio through JUCE, not
    # Essentia, so nothing is lost here. src/wscript drops audiocontext.cpp the
    # same way when libav is absent.
    'AudioWriter', 'MonoWriter',
]

# Core sources excluded on Windows. ringbufferimpl.h talks to pthread directly
# and has no Win32 branch (unlike threading.h, which does). Nothing outside
# these three files references the RingBuffer algorithms, and Audionaut uses
# PoolStorage rather than ring buffers, so dropping them costs nothing and
# avoids either a pthreads dependency or patching the submodule.
WINDOWS_EXCLUDED_CORE = [
    'ringbufferinput', 'ringbufferoutput', 'ringbuffervectoroutput',
    # Encoder-side FFmpeg wrapper, used only by AudioWriter/MonoWriter (both in
    # IGNORED_ALGORITHMS). Uses AVCodec::sample_fmts, gone in FFmpeg 7.1+.
    'audiocontext',
]


def cmake_path(path):
    """CMake wants forward slashes even on Windows."""
    return path.replace('\\', '/')


def main():
    if len(sys.argv) != 3:
        print('usage: generate_essentia_build.py <essentia-root> <output-dir>', file=sys.stderr)
        return 2

    essentia_root = os.path.abspath(sys.argv[1])
    out_dir = os.path.abspath(sys.argv[2])
    src_dir = os.path.join(essentia_root, 'src')

    if not os.path.isdir(src_dir):
        print('error: no src/ under %s - is the essentia submodule checked out?' % essentia_root,
              file=sys.stderr)
        return 1

    sys.path.insert(0, essentia_root)
    from utils.algorithms_info import (get_all_algorithms, create_registration_cpp,
                                       create_version_h)

    os.makedirs(out_dir, exist_ok=True)

    version = 'unknown'
    version_file = os.path.join(essentia_root, 'VERSION')
    if os.path.isfile(version_file):
        with open(version_file) as handle:
            version = handle.read().strip()

    algorithms = get_all_algorithms(os.path.join(src_dir, 'algorithms'), root_dir=src_dir)
    total = len(algorithms)

    ignored_present = [name for name in IGNORED_ALGORITHMS if name in algorithms]
    for name in ignored_present:
        del algorithms[name]

    # version.h is reached as "version.h" from src/essentia/config.h via the
    # include path, so putting it in the build dir works exactly like waf
    # putting it in src/.
    create_version_h(os.path.join(out_dir, 'version.h'), version, 'unknown')

    reg_cpp = os.path.join(out_dir, 'essentia_algorithms_reg.cpp')
    create_registration_cpp(algorithms, reg_cpp, use_streaming=True)

    # algorithms_info.py builds the #include paths with os.path.join, so on
    # Windows they come out as "algorithms\spectral\spectrum.h". MSVC happens to
    # accept backslashes in a header-name, but the standard leaves it undefined
    # (and "\a", "\b", "\f", "\n", "\r", "\t", "\v" would be escapes if a
    # compiler treated the token as a string literal). Normalise to forward
    # slashes, which every compiler accepts on every platform.
    with open(reg_cpp, encoding='utf8') as handle:
        registration = handle.read()
    fixed = []
    for line in registration.splitlines(True):
        if line.lstrip().startswith('#include'):
            line = line.replace('\\', '/')
        fixed.append(line)
    with open(reg_cpp, 'w', encoding='utf8') as handle:
        handle.write(''.join(fixed))

    # Core library sources: essentia/**/*.cpp, minus the ring buffers. libav and
    # libyaml are both present, so audiocontext.cpp and the yaml/jsonconvert
    # sources that src/wscript drops when they are missing all stay in.
    core_sources = []
    for root, _dirs, files in os.walk(os.path.join(src_dir, 'essentia')):
        for filename in files:
            if not filename.endswith('.cpp'):
                continue
            stem = os.path.splitext(filename)[0]
            if stem in WINDOWS_EXCLUDED_CORE:
                continue
            core_sources.append(os.path.join(root, filename))

    # One .cpp per kept algorithm, exactly as src/wscript does via algo['source'].
    algo_sources = sorted({os.path.join(src_dir, algo['source']) for algo in algorithms.values()})

    # 3rd-party code, gated on the algorithms that need it - same conditions as
    # src/wscript. KissFFT is not needed because we build against FFTW.
    third_party = []
    if 'Spline' in algorithms or 'CubicSpline' in algorithms:
        third_party.append(os.path.join(src_dir, '3rdparty', 'spline', 'splineutil.cpp'))
    if 'NNLSChroma' in algorithms:
        third_party.append(os.path.join(src_dir, '3rdparty', 'nnls', 'nnls.c'))
    if 'SNR' in algorithms:
        bessel = os.path.join(src_dir, '3rdparty', 'cephes', 'bessel')
        third_party += [os.path.join(bessel, f) for f in sorted(os.listdir(bessel))
                        if f.endswith('.cpp')]

    with open(os.path.join(out_dir, 'essentia_sources.cmake'), 'w') as handle:
        handle.write('# Generated by generate_essentia_build.py - do not edit.\n')
        handle.write('# %d of %d algorithms included.\n\n' % (len(algorithms), total))
        for name, paths in (('ESSENTIA_CORE_SOURCES', sorted(core_sources)),
                            ('ESSENTIA_ALGORITHM_SOURCES', algo_sources),
                            ('ESSENTIA_3RDPARTY_SOURCES', third_party)):
            handle.write('set(%s\n' % name)
            for path in paths:
                handle.write('    "%s"\n' % cmake_path(path))
            handle.write(')\n\n')
        handle.write('set(ESSENTIA_GENERATED_SOURCES\n    "%s"\n)\n'
                     % cmake_path(reg_cpp))

    print('essentia: %d/%d algorithms, %d core + %d algorithm + %d 3rdparty sources'
          % (len(algorithms), total, len(core_sources), len(algo_sources), len(third_party)))
    print('essentia: ignored %d algorithms (%s)'
          % (len(ignored_present), ', '.join(sorted(ignored_present))))
    return 0


if __name__ == '__main__':
    sys.exit(main())
