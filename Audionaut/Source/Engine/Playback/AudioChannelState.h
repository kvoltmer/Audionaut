//    Audionaut - Audio editing application for multitrack recordings.
//    Copyright (C) 2025 Klaus Voltmer
//
//    Audionaut uses a GPL/commercial licence - see LICENCE.md for details.


#pragma once

#include <JuceHeader.h>

namespace audium
{
class AudioChannelState {
public:
    
    AudioChannelState()
    {
        reset();
    }
    
    void reset()
    {
        gain = 1.0;
        mute = false;
        solo = false;
        channelLevel = 0.0;
        record = false;
        monitor = false;
    }
    
    std::atomic<float> channelLevel;
    float gain;
    bool mute;
    bool solo;
    bool record;
    bool monitor;
};

}
