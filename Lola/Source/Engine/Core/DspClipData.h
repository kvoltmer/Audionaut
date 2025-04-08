//    Audionaut - Audio editing application for multitrack recordings.
//    Copyright (C) 2025 Klaus Voltmer
//
//    Audionaut uses a GPL/commercial licence - see LICENCE.md for details.

#pragma once

#include "Engine/Group/AudioClipData.h"

namespace audium {

struct DspClipData
{
    // The audio clip data
    AudioClipData clipData;
    
    // Indicate if clip is active
    bool active = false;
    
    float clipGain = 1.f;
    
    double clipFadeInClocks = 0.0;
    double clipFadeOutClocks = 0.0;
    
    int transportSourceIndex = -1;
    
};

} // namespace audium
