/*
 *    Audionaut - Audio editing application for multitrack recordings.
 *    Copyright (C) 2025 Klaus Voltmer
 *
 *    Audionaut uses a GPL/commercial licence - see LICENCE.md for details.
 *
 *  Minimal <unistd.h> stand-in for MSVC.
 *
 *  Essentia's src/essentia/debugging.h includes <unistd.h> for a single call:
 *  isatty(2), to decide whether to colour its output. MSVC has no <unistd.h>,
 *  but provides the same function as _isatty() in <io.h>.
 *
 *  This directory is placed on the include path ahead of nothing else, so the
 *  angle-bracket include resolves here and the Essentia submodule stays
 *  unmodified - the other approach would be patching the working tree the way
 *  build_essentia.sh has to on macOS.
 */

#ifndef AUDIONAUT_MSVC_UNISTD_SHIM_H
#define AUDIONAUT_MSVC_UNISTD_SHIM_H

#ifdef _MSC_VER

#include <io.h>
#include <process.h>

#ifndef isatty
#define isatty _isatty
#endif

#ifndef fileno
#define fileno _fileno
#endif

#endif // _MSC_VER

#endif // AUDIONAUT_MSVC_UNISTD_SHIM_H
