//    Audionaut - Audio editing application for multitrack recordings.
//    Copyright (C) 2025 Klaus Voltmer
//
//    Audionaut uses a GPL/commercial licence - see LICENCE.md for details.

#pragma once

namespace audium
{

#ifndef MAX_VOICES
    #define MAX_VOICES 64
#endif

#ifndef MAX_AUDIO_CHANNELS
    #if JUCE_DEBUG
        #define MAX_AUDIO_CHANNELS 64 // improve compile time
    #else
        #define MAX_AUDIO_CHANNELS 128
    #endif
#endif

} // namespace audium
