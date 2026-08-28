# Installation

Downloads are linked from [audionaut.app](https://audionaut.app); the release
files themselves live on the project's GitHub Releases page.

## Windows

Download `Audionaut-<version>-Setup.exe` and run it. The installer places
Audionaut in the usual program locations and creates a Start-menu entry.

## Linux

Two packages are published per release:

- **AppImage** (`Audionaut-<version>-x86_64.AppImage`) — make it executable
  (`chmod +x`) and run it directly; no installation required.
- **Debian package** (`.deb`) — install with your package manager, e.g.
  `sudo apt install ./audionaut_<version>_amd64.deb`.

## macOS

There is currently no prebuilt macOS download — Audionaut builds from source
with Xcode in a few steps. Follow the **Checkout** and **Build** sections of
the repository [README](../../README.md).

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
