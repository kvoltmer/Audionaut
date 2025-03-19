//    Lola - Audio editing application for multitrack recordings.
//    Copyright (C) 2025 Klaus Voltmer
//
//    Lola uses a GPL/commercial licence - see LICENCE.md for details.

#pragma once

#include <JuceHeader.h>
#include <nlohmann/json.hpp>

using json = nlohmann::json;

namespace audium {

struct AudioClipData
{
    typedef class juce::Range<double> tRange;
    
    // The absolute transport position
    double absolutePositionClocks = 0.0;
    
    // The start and end in seconds
    tRange regionData;
    
};

// custom to_json method will be automatically called by the json constructor
inline void to_json(json& j, const AudioClipData& d) {
    j = json{   {"position_clocks", d.absolutePositionClocks},
        {"start", d.regionData.getStart()},
        {"end", d.regionData.getEnd()} };
}

// custom from_json method will be automatically called by the json constructor
inline void from_json(const json& j, AudioClipData& d) {
    j.at("position_clocks").get_to(d.absolutePositionClocks);
    d.regionData.setStart(j.at("start").get<double>());
    d.regionData.setEnd(j.at("end").get<double>());
}

} // namespace audium
