//    Lola - Audio editing application for multitrack recordings.
//    Copyright (C) 2025 Klaus Voltmer
//
//    This program is free software: you can redistribute it and/or modify
//    it under the terms of the GNU General Public License as published by
//    the Free Software Foundation, either version 3 of the License, or
//    (at your option) any later version.
//
//    This program is distributed in the hope that it will be useful,
//    but WITHOUT ANY WARRANTY; without even the implied warranty of
//    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
//    GNU General Public License for more details.
//
//    You should have received a copy of the GNU General Public License
//    along with this program.  If not, see <https://www.gnu.org/licenses/>.

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

