# Installation

All downloads are linked from
[audionaut.app](https://audionaut.app/download/).

## macOS

Download **Audionaut App** from the
[Mac App Store](https://apps.apple.com/us/app/id6743627933).

Building the GPL version from source with Xcode is also supported — follow
the **Checkout** and **Build** sections of the repository
[README](../../README.md).

## Windows

Download the installer (`Audionaut-<version>-Setup.exe`) from
[GitHub Releases](https://github.com/kvoltmer/Audionaut/releases) and run
it. The installer is not yet code-signed, so Windows SmartScreen may warn on
first launch — choose **More info** → **Run anyway**.

## Linux

Two packages are published per release on
[GitHub Releases](https://github.com/kvoltmer/Audionaut/releases):

- **AppImage** (`Audionaut-<version>-x86_64.AppImage`) — no installation
  needed; make it executable (`chmod +x`) and run it.
- **Debian package** (`.deb`, for Debian/Ubuntu and derivatives) — install
  with apt, which also resolves dependencies:
  `sudo apt install ./audionaut_<version>_amd64.deb`.

## First launch

Two things happen the first time you start Audionaut:

1. **Usage statistics consent.** Audionaut asks whether it may report
   anonymous usage statistics. Nothing is sent unless you agree, and you can
   change your answer anytime in *Settings → Privacy*. See
   [Privacy](12-privacy.md) for exactly what this covers.
2. **Audio device.** Check *Settings → Audio* to pick your output (and, for
   recording, input) device, sample rate and buffer size.

On macOS, *Settings…* is in the application menu (**Cmd+,**); on Windows and
Linux it is in the *File* menu.
