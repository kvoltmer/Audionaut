# LAME in Audionaut

The MP3 encoder used by Audionaut's MP3 export.

- **Upstream:** LAME, https://lame.sourceforge.io/ (sources in SVN on SourceForge;
  there is no official git repository)
- **Version:** 4.0, released 2026-07-11
- **Source:** the official release tarball
  https://sourceforge.net/projects/lame/files/lame/4.0/lame-4.0.tar.gz
  - SHA-256 `3df5124d5ad3a98312ffd7ba6a9b36230e4f8a3e66d3ce0f425e336c32d216eb`
  - SHA-1 `e3630adf399d8917d4da19b96937b7f9b12774ea`
  - SourceForge publishes no checksum. As a cross-check, `libmp3lame/` and `include/lame.h`
    are byte-identical to the 4.0 release commit `5a2d347159` of the git mirror
    https://github.com/enzo1982/lame.
- **Licence:** GNU LGPL version 2 or later. See `COPYING` and `LICENSE`.

## What is here

Unmodified files from the tarball:

- `include/lame.h`
- `libmp3lame/*.c`, `libmp3lame/*.h`, `libmp3lame/vector/lame_intrin.h`
- `COPYING`, `LICENSE`, `README`

These are left out because only the encoder is compiled:

- `libmp3lame/mpglib_interface.c` and `mpglib/` (the decoder: Audionaut decodes MP3 with JUCE)
- `libmp3lame/vector/xmm_quantize_sub.c` and `libmp3lame/i386/` (SSE/NASM code paths)
- the frontend, the DLL/ACM/DirectShow wrappers, docs, tests and the build files

Added by Audionaut:

- `audionaut/lame_config.h` stands in for the autotools/MSVC configuration headers. It is
  force-included into each LAME source, so `HAVE_CONFIG_H` is not defined.

To update, replace the files above from a newer release tarball, then update this file's
version and checksums.
