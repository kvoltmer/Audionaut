/*
  ==============================================================================

    AudioChannelData.h
    Created: 26 Feb 2024 11:45:46am
    Author:  Klaus Voltmer

  ==============================================================================
*/

#pragma once

#include <nlohmann/json.hpp>

using json = nlohmann::json;

//==============================================================================
struct AudioChannelData
{
    int height = 100;
    float gain = 1.0f;
};

// custom to_json method will be automatically called by the json constructor
inline void to_json(json& j, const AudioChannelData& d) {
    j = json{   {"height", d.height},
                {"gain", d.gain}};
}

// custom from_json method will be automatically called by the json constructor
inline void from_json(const json& j, AudioChannelData& d) {
    j.at("height").get_to(d.height);
    if (j.contains("gain")) {
        d.gain = j.at("gain").get<float>();
    }
}

