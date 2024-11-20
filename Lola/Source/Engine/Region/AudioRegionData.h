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
    tRange      range;
    // TODO: region_id is obsolete. see: AudioRegionContainer::getRegionId
    int         region_id = -1; // invalid
};

inline void to_json(json& j, const AudioRegionData& r) {
    j = json{   {"name", r.name},
                {"start", r.range.getStart()},
                {"end", r.range.getEnd()},
                {"id", r.region_id}
    };
}

inline void from_json(const json& j, AudioRegionData& r) {
    j.at("name").get_to(r.name);
    r.range.setStart(j.at("start").get<double>());
    r.range.setEnd(j.at("end").get<double>());
    
    if (j.contains("id"))
        j.at("id").get_to(r.region_id);
    
}
