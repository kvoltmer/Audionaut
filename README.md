# Audionaut

[![License: GPL v3](https://img.shields.io/badge/License-GPLv3-blue.svg)](LICENSE.md)
[![macOS tests](https://github.com/kvoltmer/Audionaut/actions/workflows/cmake-single-platform.yml/badge.svg)](https://github.com/kvoltmer/Audionaut/actions/workflows/cmake-single-platform.yml)
[![Linux build](https://github.com/kvoltmer/Audionaut/actions/workflows/makefile.yml/badge.svg)](https://github.com/kvoltmer/Audionaut/actions/workflows/makefile.yml)
[![Sponsor](https://img.shields.io/badge/%E2%9D%A4-Sponsor-ff69b4)](https://github.com/sponsors/kvoltmer)

Audionaut is a free, open-source desktop application for effortless audio editing and recording. Whether you're working on music, podcasts, or multitrack recordings, it gives you precise cutting, per-track playlists, flexible multi-channel support, and clean exports — without the weight and complexity of a full DAW. Audionaut is written in modern C++ on the [JUCE](https://juce.com) framework and runs natively on Windows, macOS, and Linux.

Ready-made downloads and more information: **[audionaut.app](https://audionaut.app)**

### Checkout

Make sure to clone with submodules:

```
git clone --recursive https://github.com/kvoltmer/Audionaut.git
```

or once cloned do:

```
git submodule update --init --recursive
```

### Essentia (audio analysis)

The analysis features (BIC segmentation, onset detection, beat tracking) link against a static build of the [Essentia](https://essentia.upf.edu) submodule. Build it once after checking out the submodules:

```
./Audionaut/Builds/build_essentia.sh
```

The script builds Essentia's 3rd-party dependencies statically (slow, skipped on re-runs; force with `FORCE_3RDPARTY=1`), then configures and builds Essentia itself with its waf build system into `Submodules/essentia/build`. It patches Essentia's Linux-oriented 3rd-party build scripts in the working tree as needed for macOS/Apple Silicon, so the essentia submodule will show as dirty afterwards — don't commit those changes.

Prerequisites:

- **Python ≤ 3.11** — Essentia's bundled waf needs `distutils`, removed in Python 3.12. The script picks a suitable interpreter automatically; override with `PYTHON=... ./Audionaut/Builds/build_essentia.sh`.
- **pkg-config** (e.g. `brew install pkg-config`)
- **CMake 3.x recommended** — some 3rd-party deps ship very old CMakeLists that CMake 4.x refuses (e.g. `pip install "cmake~=3.31.0"`).

Building Essentia is optional: without it, `ESSENTIA_ENABLED` auto-detects off and the app and tests compile with the analysis features disabled.

### Build

**Xcode** — open the Xcode project located here:

```
Audionaut/Builds/MacOSX/Audionaut.xcodeproj
```

**Visual Studio 2026** — open the Visual Studio solution located here:

```
Audionaut/Builds/VisualStudio2026/Audionaut.sln
```

**Linux Makefile** — install dependencies:

```
sudo apt install libasound2-dev libjack-jackd2-dev ladspa-sdk libcurl4-openssl-dev libfreetype-dev libfontconfig1-dev libx11-dev libxcomposite-dev libxcursor-dev libxext-dev libxinerama-dev libxrandr-dev libxrender-dev libwebkit2gtk-4.1-dev libglu1-mesa-dev mesa-common-dev
```

then compile:

```
cd Audionaut/Builds/LinuxMakefile/
make CONFIG=Release -j8
```

### Tests

See the <a href="Audionaut/Catch2Tests/README.md">Catch2 tests README</a>.

### License

Audionaut is dual-licensed under GPL3 (or later) and a commercial license — see <a href="LICENSE.md">LICENSE.md</a> for details.

### Contributing

Contributions are welcome — see <a href="CONTRIBUTING.md">CONTRIBUTING.md</a> for how to get started and the required Contributor License Agreement (<a href="CLA.md">CLA.md</a>).

### Support

If Audionaut is useful to you, consider [sponsoring its development](https://github.com/sponsors/kvoltmer) — sponsorship directly funds development time and keeps the project sustainable.
