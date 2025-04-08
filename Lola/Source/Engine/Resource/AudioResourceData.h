//    Audionaut - Audio editing application for multitrack recordings.
//    Copyright (C) 2025 Klaus Voltmer
//
//    Audionaut uses a GPL/commercial licence - see LICENCE.md for details.

#pragma once

#include <JuceHeader.h>
#include <nlohmann/json.hpp>

using json = nlohmann::json;

namespace audium {

struct AudioResourceData
{
    typedef class juce::Range<double> tRange;
    
    std::string url;
    float gain;
    int channelPos;
};

NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(AudioResourceData,
                                   url,
                                   gain,
                                   channelPos);

} // namespace audium
