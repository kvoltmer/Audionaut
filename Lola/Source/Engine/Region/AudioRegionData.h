/*
  ==============================================================================

    AudioRegionData.h
    Created: 23 Feb 2024 10:14:29am
    Author:  Klaus Voltmer

  ==============================================================================
*/

#pragma once

#include <JuceHeader.h>
#include <nlohmann/json.hpp>

using json = nlohmann::json;

//==============================================================================
struct AudioRegionData
{
    typedef class juce::Range<double> tRange;
    
    std::string name;
    tRange      regionData;
};

inline void to_json(json& j, const AudioRegionData& r) {
    j = json{   {"name", r.name},
                {"start", r.regionData.getStart()},
                {"end", r.regionData.getEnd()}
    };
}

inline void from_json(const json& j, AudioRegionData& r) {
    j.at("name").get_to(r.name);
    r.regionData.setStart(j.at("start").get<double>());
    r.regionData.setEnd(j.at("end").get<double>());
}
