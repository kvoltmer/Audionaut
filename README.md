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

### demucs.cpp (stem separation)

Stem separation runs [demucs.cpp](https://github.com/sevagh/demucs.cpp), a C++ port of Meta's Demucs, compiled straight from the `Submodules/demucs.cpp` submodule — no separate build step. It needs the submodule's vendored Eigen (`Submodules/demucs.cpp/vendor/eigen`, a nested submodule that `--recursive` fetches); the CMake build detects it and sets `AUDIONAUT_ENABLE_DEMUCS` accordingly. The model weights are not in the repository: the app downloads them on first use into its `Models` folder, or point the CLI at a copy with `--model`.

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

### Command-line tool (audionaut-cli)

`audionaut-cli` gives scripts, CI and AI agents headless access to `.audium`
projects — no GUI, no audio device. It builds alongside the tests:

```
cmake -B build -S Audionaut/Catch2Tests
cmake --build build -j8 --target AudionautCli
./build/AudionautCli_artefacts/AudionautCli --help
```

Every command takes `--json` to emit exactly one machine-readable result
envelope on stdout (`{"ok": true, "result": ...}` or `{"ok": false, "error":
...}`) with all logging on stderr, plus `--quiet`. Exit codes: `0` success,
`1` operation failed, `2` usage error, `3` feature unavailable in this build
(e.g. `analyze` without Essentia). Option values may be given as `--opt value`
or `--opt=value`.

```
audionaut-cli create  song.audium --channels 2
audionaut-cli import  song.audium take1.wav take2.wav --position 4.5
audionaut-cli info    song.audium --json
audionaut-cli analyze song.audium --types sbic,beat_degara
audionaut-cli auto-edit song.audium --track 0 --measures 4
audionaut-cli assemble  song.audium --duration 60 --mode random
audionaut-cli split   song.audium --at 23              # bar 23; --unit beats|seconds|clocks
audionaut-cli create-region song.audium --name chorus --start 17 --end 25
audionaut-cli set-region  song.audium --region chorus --rename drop --length 8
audionaut-cli place-clip  song.audium --region drop --at 33
audionaut-cli move-clip   song.audium --region drop --to 41
audionaut-cli remove-clip song.audium --at 41 --track 0
audionaut-cli clip-gain   song.audium --region drop --gain -6 --db
audionaut-cli clip-fades  song.audium --region drop --fade-in 1 --fade-out 2 --unit beats
audionaut-cli separate  song.audium --track 1                  # Drums/Bass/Other/Vocals tracks; needs the Demucs model
audionaut-cli export  song.audium -o mix.wav --sample-rate 48000 --bit-depth 24
```

A typical agent flow: `create` → `import` → `analyze` → `auto-edit`/`assemble`
→ `export`, checking `ok` in each `--json` envelope. Projects written by the
CLI open in the GUI app and vice versa.

End-user documentation lives in the **[User Manual](docs/manual/README.md)**.

CLI invocations report anonymous usage statistics under the same strictly
opt-in consent as the app (one `cli_command` event: verb and exit code) —
nothing is sent unless consent was granted in the app's settings. Set
`AUDIONAUT_DISABLE_ANALYTICS=1` to switch the CLI's reporting off regardless
(CI environments, scripts).

The **main Audionaut app** also accepts the same verbs (Projucer-style): run
the app binary with a verb and it executes headlessly and quits with the
command's exit code, even while a GUI instance is open — a file argument or
no arguments launches the GUI as usual.

```
./Audionaut.app/Contents/MacOS/Audionaut export ~/Music/song.audium -o ~/Music/mix.wav
```

Note: the macOS app is sandboxed, so its in-app CLI can only reach
entitlement-covered locations such as `~/Music`; the standalone
`audionaut-cli` build has no such restriction. On Windows, the app is a GUI
program — the shell prompt returns immediately and output interleaves with
it; prefer `audionaut-cli` for scripting there.

### License

Audionaut is dual-licensed under GPL3 (or later) and a commercial license — see <a href="LICENSE.md">LICENSE.md</a> for details.

### Contributing

Contributions are welcome — see <a href="CONTRIBUTING.md">CONTRIBUTING.md</a> for how to get started and the required Contributor License Agreement (<a href="CLA.md">CLA.md</a>).

### Support

If Audionaut is useful to you, consider [sponsoring its development](https://github.com/sponsors/kvoltmer) — sponsorship directly funds development time and keeps the project sustainable.
