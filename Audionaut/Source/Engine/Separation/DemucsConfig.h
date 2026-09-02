//    Audionaut - Audio editing application for multitrack recordings.
//    Copyright (C) 2025 Klaus Voltmer
//
//    Audionaut uses a GPL/commercial licence - see LICENCE.md for details.

#pragma once

/**
 * DEMUCS_ENABLED: whether the demucs.cpp stem-separation backend is compiled
 * in. Unlike ESSENTIA_ENABLED this is not auto-detected from headers - the
 * demucs.cpp sources are compiled in isolation (see Demucs/DemucsRunner.h),
 * so no other translation unit can see them. The build defines it: the .jucer
 * globally, Catch2Tests/CMakeLists.txt from AUDIONAUT_ENABLE_DEMUCS. Include
 * this header wherever the macro is tested so a missing define reads as off
 * instead of as a compile error.
 */
#ifndef DEMUCS_ENABLED
 #define DEMUCS_ENABLED 0
#endif
