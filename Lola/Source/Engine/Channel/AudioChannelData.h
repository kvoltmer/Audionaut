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
    int height  = 100;
    float gain  = 1.0f;
    float pan   = 0.f;
    bool mute   = false;
    bool solo   = false;
};

// custom to_json method will be automatically called by the json constructor
inline void to_json(json& j, const AudioChannelData& data) {
    j = json{   {"height", data.height},
                {"gain", data.gain},
                {"pan", data.pan},
                {"mute", data.mute},
                {"solo", data.solo}
            };
}

// custom from_json method will be automatically called by the json constructor
inline void from_json(const json& j, AudioChannelData& data) {
    
    if (j.contains("height"))
        data.height = j.at("height").get<int>();
        
    if (j.contains("gain"))
        data.gain = j.at("gain").get<float>();
    
    if (j.contains("pan"))
        data.pan = j.at("pan").get<float>();
    
    if (j.contains("mute"))
        data.mute = j.at("mute").get<bool>();
    
    if (j.contains("solo")) {
        data.solo = j.at("solo").get<bool>();
    }
}

