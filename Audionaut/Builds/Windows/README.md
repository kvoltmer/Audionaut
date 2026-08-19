# Essentia on Windows (MSVC)

Builds Essentia as a static MSVC library so Audionaut's analysis features — BIC
segmentation, onset detection, beat tracking — work on Windows. Run
[`../build_essentia_windows.ps1`](../build_essentia_windows.ps1); everything
here is invoked by that script.

## Why not build_essentia.sh

`build_essentia.sh` drives Essentia's own waf build. That build has no working
Windows path:

- **The waf win32 branch is dead code.** In `Submodules/essentia/wscript`, the
  `elif sys.platform == 'win32':` branch prints `"Building on win32"` and does
  nothing else — its entire body sits inside a `"""` triple-quoted string.
- **The 3rd-party scripts are bash targeting MinGW.**
  `packaging/build_3rdparty_static_win32.sh` and `win32_3rdparty/*.sh` build with
  TDM-GCC against versions pinned around 2013 (`fftw-3.3.3`, `libav-0.8.9`).
- **MinGW output is unusable here anyway.** Those scripts emit `ar` archives;
  `link.exe` cannot consume them, and Audionaut is built with MSVC.
- The checked-in `essentiaVC80.vcproj` files are Visual Studio 2005.

So this directory replaces waf with CMake and replaces `packaging/` with vcpkg.

## How it works

| File | Role |
| --- | --- |
| `generate_essentia_build.py` | Produces what waf would generate: `version.h`, the `AlgorithmFactory` registration table, and the source lists |
| `CMakeLists.txt` | Compiles those sources into `essentia.lib` against vcpkg dependencies |
| `VcpkgDeps.cmake` | Locates the vcpkg dependencies; shared with `Catch2Tests/CMakeLists.txt` so the two cannot drift |
| `compat/unistd.h` | Supplies the one POSIX header Essentia's `debugging.h` includes |

### Changes to the submodule

Exactly **one** file is patched in the Essentia working tree —
`src/essentia/roguevector.h` — by `build_essentia_windows.ps1`. Like the macOS
patches in `build_essentia.sh`, it is idempotent and **must not be committed**.

`RogueVector` aliases externally-owned memory by reaching into `std::vector`'s
internals, with a separate implementation per standard library. Its `OS_WIN32`
branch calls `_Myfirst()` / `_Mylast()` / `_Myend()` as member *functions*,
which is how the MSVC STL exposed them up to Visual Studio 2013; they have been
data members inside a private `_Mypair` ever since. That single mismatch
accounts for roughly 2000 compile errors. The patch switches Windows to the
same pointer-punning the file already uses for clang — no more supported, but
no less.

It cannot be avoided by shadowing the header: `roguevector.h` is included with
quotes from files inside `essentia/` itself, so the submodule's own copy always
wins the lookup regardless of include path order.

Everything else needs no source changes. Generated files go to the build
directory, and the one POSIX dependency is satisfied by `compat/unistd.h` on
the include path.

### Dependencies

From vcpkg, mirroring `build_essentia.sh`'s
`--lightweight=fftw,libav,libsamplerate,yaml`:

`fftw3` · `ffmpeg` · `libsamplerate` · `libyaml` · `eigen3`

The triplet is **`x64-windows-static-md`** — static libraries against the
*dynamic* CRT (`/MD`). That matches both `Audionaut_App.vcxproj` and the JUCE
CMake test target. Getting this wrong does not fail at configure time; it
surfaces much later as duplicate-symbol or `_ITERATOR_DEBUG_LEVEL` mismatch
errors at link.

The same trap applies within a correct triplet: vcpkg installs release
libraries in `<prefix>/lib` and debug ones in `<prefix>/debug/lib` under
identical names, so a plain `find_library` will happily link a debug FFmpeg
into a Release build. `VcpkgDeps.cmake` resolves both and selects with a
generator expression; do not replace it with bare `find_library` calls.

Two dependency quirks are also handled there:

- **`YAML_DECLARE_STATIC`** must be defined. Without it libyaml's headers
  declare every entry point `__declspec(dllimport)` and the references come out
  as unresolvable `__imp_yaml_*`.
- **`crypt32` and `ncrypt`** are required by FFmpeg's schannel TLS backend
  (`tls_schannel.o`), alongside the more obvious `ws2_32`/`secur32`/`bcrypt`
  and the Media Foundation and DirectShow libraries.

### What is left out

255 of Essentia's 282 algorithms are built. Most of the 27 exclusions mirror
the `ALGOIGNORE` list `src/wscript` accumulates for this dependency set:

- `FFTK*` / `FFTA*` — the KissFFT and Apple Accelerate FFT variants, dropped
  because FFTW is present (`src/wscript` does the same). Accelerate is
  macOS-only regardless.
- `MetadataReader`, `MusicExtractor`, `FreesoundExtractor` — need TagLib
- `GaiaTransform`, `MusicExtractorSVM` — need Gaia2
- `Chromaprinter` — needs Chromaprint
- `TensorflowPredict*`, `PitchCREPE`, `TempoCNN` — need TensorFlow

Two exclusions are specific to this port: **`AudioWriter` and `MonoWriter`**.
They are the only users of `audiocontext.cpp`, which reads
`AVCodec::sample_fmts` — removed from the public struct in FFmpeg 7.1 in favour
of `avcodec_get_supported_config()`. Essentia 2.1-beta6 predates that change
and only ever checks for `avcodec >= 55.34.1` (FFmpeg 2.x), while vcpkg ships
FFmpeg 9. Audio *reading* is unaffected: `AudioLoader` and `MonoLoader` compile
against FFmpeg 9 unchanged. Audionaut writes audio through JUCE rather than
Essentia, so nothing is lost — but restoring these would mean porting
`audiocontext.cpp` to the current FFmpeg encoder API.

All eight algorithms Audionaut uses (`FrameCutter`, `MFCC`, `MonoLoader`,
`OnsetRate`, `RhythmExtractor2013`, `SBic`, `Spectrum`, `Windowing`) are
included.

Three core sources are also skipped: `ringbufferinput.cpp`,
`ringbufferoutput.cpp` and `ringbuffervectoroutput.cpp`. They use pthreads
directly via `ringbufferimpl.h`, which — unlike `threading.h` — has no Win32
branch. Nothing else in Essentia references them and Audionaut uses
`PoolStorage`, so excluding them avoids taking on a pthreads dependency or
patching the submodule.

## Building the app against it

`essentia.lib` feeds two targets: the Catch2 test build (configured with
`-DAUDIONAUT_VCPKG_PREFIX=...`, see above) and the app itself. The app picks it
up through the VS2026 exporter in `Audionaut.jucer` — re-save the `.jucer` in
the Projucer after changing it, as with any exporter change.

The segmenters (`BeatSegmenter`, `OnsetSegmenter`, `SBicSegmenter`) select their
implementation with `__has_include(<essentia/algorithmfactory.h>)` and
`__has_include(<unsupported/Eigen/CXX11/Tensor>)`, so nothing has to be switched
on by hand: if the include paths resolve, `ESSENTIA_ENABLED` becomes 1 and the
real implementations compile. If they do not, the app still builds and the
analysis features are inert stubs. A Windows app build that silently lacks
analysis means the include paths did not resolve — check that
`build_essentia_windows.ps1` has been run.

Two details differ from the macOS and Linux exporters:

- **vcpkg lives outside the repo**, so its location cannot be a fixed relative
  path. Both configurations list `$(AUDIONAUT_VCPKG_PREFIX)` first and the
  script's default location (`vcpkg` beside the repo) second. Set the
  environment variable if vcpkg is anywhere else; MSBuild expands an undefined
  property to an empty string, leaving the entry harmlessly unresolvable.
- **The library path is per-configuration**, because vcpkg installs release
  libraries in `<prefix>/lib` and debug ones in `<prefix>/debug/lib` under
  identical names — the same trap `VcpkgDeps.cmake` handles with generator
  expressions. Debug searches `debug/lib`, Release searches `lib`, and each
  points at its matching `build-msvc/lib/<config>`. This means the app
  configuration must match the one Essentia was built with:
  `build_essentia_windows.ps1 -Configuration Debug` is needed before a Debug
  app build, and defaults to Release otherwise.

## Keeping this in sync

The algorithm exclusions and source-selection rules are transcribed from
`Submodules/essentia/src/wscript`. If the Essentia submodule is updated, check
that file for changes to its `ALGOIGNORE` handling and the `sources` glob near
the `build()` function, and mirror them in `generate_essentia_build.py`.
