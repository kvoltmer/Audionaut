//    Audionaut - Audio editing application for multitrack recordings.
//    Copyright (C) 2025 Klaus Voltmer
//
//    Audionaut uses a GPL/commercial licence - see LICENCE.md for details.

#include <AudioToolbox/AudioToolbox.h>

//==============================================================================
// "Audionaut Tests: Crash On Scan" - an Audio Unit effect that takes the
// process down the moment a host opens it, which a plugin scan does to read
// its description. The component registers like any effect (Info.plist), so
// the system lists it; the crash is in its factory, the first thing of the
// plugin the host runs.
//==============================================================================

extern "C" __attribute__ ((visibility ("default")))
void* AudionautTestCrashOnScanFactory (const AudioComponentDescription*)
{
    volatile int* nowhere = nullptr;
    *nowhere = 1;   // SIGSEGV: no exception a host could catch
    return nullptr;
}
