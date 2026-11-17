//    Audionaut - Audio editing application for multitrack recordings.
//    Copyright (C) 2025 Klaus Voltmer
//
//    Audionaut uses a GPL/commercial licence - see LICENCE.md for details.

#include <AudioToolbox/AudioToolbox.h>

//==============================================================================
// "Audionaut Tests: Empty" - an Audio Unit effect with nothing in it. The
// component registers like any effect (Info.plist), so the system lists it,
// and its factory hands the host no plugin: opening it fails, quietly. A
// scan finds no plugin in it and has nothing to hold against it.
//==============================================================================

extern "C" __attribute__ ((visibility ("default")))
void* AudionautTestEmptyFactory (const AudioComponentDescription*)
{
    return nullptr;
}
