//    Audionaut - Audio editing application for multitrack recordings.
//    Copyright (C) 2025 Klaus Voltmer
//
//    Audionaut uses a GPL/commercial licence - see LICENCE.md for details.

// Bungee's own version.cpp wants a BUNGEE_VERSION define with embedded
// quotes, which the per-file compiler flags cannot carry portably across
// the three IDE exporters. This stands in for it; the submodule's
// version.cpp is not compiled.
#include "StretchBackend.h"

#if STRETCH_BUNGEE_ENABLED
namespace Bungee {
const char* versionDescription = "0.0.0-audionaut";
}
#endif
