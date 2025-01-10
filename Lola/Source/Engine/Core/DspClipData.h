#pragma once

#include "Engine/Group/AudioClipData.h"

namespace audium {

struct DspClipData
{
    // The audio clip data
    AudioClipData clipData;
    
    // Indicate if clip is active
    bool active = false;
    
    float clip_gain = 1.f;
    
    int transportSourceIndex = -1;
    
};

} // namespace audium
