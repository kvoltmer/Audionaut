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
