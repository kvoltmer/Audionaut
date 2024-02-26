/*
  ==============================================================================

    AudioClipData.h
    Created: 26 Feb 2024 9:31:15am
    Author:  Klaus Voltmer

  ==============================================================================
*/

#pragma once

#include <JuceHeader.h>
#include <nlohmann/json.hpp>

using json = nlohmann::json;

//==============================================================================
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
    j = json{   {"positionClocks", d.absolutePositionClocks},
                {"start", d.regionData.getStart()},
                {"end", d.regionData.getEnd()} };
}

// custom from_json method will be automatically called by the json constructor
inline void from_json(const json& j, AudioClipData& d) {
    j.at("positionClocks").get_to(d.absolutePositionClocks);
    d.regionData.setStart(j.at("start").get<double>());
    d.regionData.setEnd(j.at("end").get<double>());
}
