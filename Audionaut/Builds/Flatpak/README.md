# Flatpak packaging

Skeleton for a Flatpak of Audionaut, aimed at Flathub. Status: **not yet
built end to end** — the manifest encodes the plan; expect iteration on the
Essentia module in particular.

## Files

- `app.audionaut.Audionaut.yml` — flatpak-builder manifest. Builds Essentia
  (with its 3rd-party tarballs pre-seeded for Flathub's offline builds),
  then the app via the LinuxMakefile, and installs the desktop entry, icon,
  MIME definition and metainfo renamed to the app id.
- `app.audionaut.Audionaut.metainfo.xml` — AppStream metadata, required by
  Flathub. Also suitable for the .deb (`/usr/share/metainfo`).

## Local build

```
flatpak install -y flathub org.gnome.Platform//48 org.gnome.Sdk//48
flatpak-builder --force-clean --user --install build-dir \
    Audionaut/Builds/Flatpak/app.audionaut.Audionaut.yml
flatpak run app.audionaut.Audionaut
```

Note the git source builds the **pinned release tag**, not your working
tree. For a working-tree build, point the git source at a local path/branch
temporarily (`url: ../../..` style paths work with flatpak-builder).

## Known gaps / TODOs

- **Untested**: the offline curl shim around Essentia's `build_*.sh`
  downloads, the setuptools/`distutils` shim for waf on Python 3.12, and
  the full compile inside the sandbox all need a first real run.
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
- **Release automation**: the git `tag`/`commit` pin must be bumped per
  release. Once on Flathub, that's a PR to the `flathub/app.audionaut.Audionaut`
  repo (can be automated from `release-linux.yml`, or via Flathub's
  external-data-checker).
- **Separate user data**: a Flatpak stores settings/models under
  `~/.var/app/app.audionaut.Audionaut/`, separate from a .deb install —
  users switching package formats re-download the Demucs model.

## Flathub submission (later)

1. Verify the `app.audionaut` developer id / `audionaut.app` domain.
2. PR the manifest (+ metainfo) to `flathub/flathub` per
   https://docs.flathub.org/docs/for-app-authors/submission.
3. Review feedback usually targets finish-args and metainfo quality.
