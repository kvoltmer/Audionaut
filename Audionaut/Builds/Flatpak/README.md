# Flatpak packaging

Flatpak packaging for Audionaut, aimed at Flathub. Verified end to end:
builds, installs and runs in the sandbox (audio via PipeWire, home access,
Essentia analysis, file import).

## Files

- `app.audionaut.Audionaut.yml` — flatpak-builder manifest. Builds Essentia
  (with its 3rd-party tarballs pre-seeded for Flathub's offline builds),
  then the app via the LinuxMakefile, and installs the desktop entry, icon,
  MIME definition and metainfo renamed to the app id.
- `app.audionaut.Audionaut.metainfo.xml` — AppStream metadata, required by
  Flathub. Also suitable for the .deb (`/usr/share/metainfo`).
- `sync-pins.py` — rewrites the manifest's git pins (app tag/commit and
  every submodule commit) from the current checkout. The release workflow
  runs it so release builds always build the pushed tag; run it by hand to
  bump the committed pins.

## Local build

```
flatpak install -y flathub org.gnome.Platform//51 org.gnome.Sdk//51
flatpak-builder --force-clean --user --install build-dir \
    Audionaut/Builds/Flatpak/app.audionaut.Audionaut.yml
flatpak run app.audionaut.Audionaut
```

Note the git source builds the **pinned release tag**, not your working
tree. For a working-tree build, point the git source at a local path/branch
temporarily (`url: ../../..` style paths work with flatpak-builder).

## Known gaps / TODOs

- **Screenshots**: metainfo points at the social-preview card as a
  placeholder; Flathub review wants real UI screenshots (PNG).
- **File access** is `--filesystem=home` because `.audium` projects are
  directory packages and JUCE does not use the document portals. Revisit
  if JUCE grows portal support.
- **Analytics**: Flathub builds from the public repo, so
  `AnalyticsCredentials.h` (the GA4 API secret) is absent and the Flatpak
  ships with analytics disabled. That is probably the right default for
  Flathub anyway — a reviewer may ask about phone-home behaviour.
- **x86_64 only**: Essentia's fftw build config passes x86 SSE2 flags;
  aarch64 needs the arm patches from `build_essentia.sh` extended to Linux.
- **Release automation**: handled for the GitHub-release bundle —
  `release-linux.yml` runs `sync-pins.py` and attaches
  `Audionaut-<version>-x86_64.flatpak` to the release. Installing a bundle
  needs the flathub remote configured for the runtime, then
  `flatpak install ./Audionaut-<version>-x86_64.flatpak`. Once on Flathub,
  releases additionally mean a pin-bump PR to the
  `flathub/app.audionaut.Audionaut` repo (automatable the same way).
- **Separate user data**: a Flatpak stores settings/models under
  `~/.var/app/app.audionaut.Audionaut/`, separate from a .deb install —
  users switching package formats re-download the Demucs model.

## Flathub submission (later)

1. Verify the `app.audionaut` developer id / `audionaut.app` domain.
2. PR the manifest (+ metainfo) to `flathub/flathub` per
   https://docs.flathub.org/docs/for-app-authors/submission.
3. Review feedback usually targets finish-args and metainfo quality.
