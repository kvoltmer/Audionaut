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

//==============================================================================
struct AudioRegionData
{
    typedef class juce::Range<double> tRange;
    
    // region name
    std::string name;
    
    // start and end [seconds]
    tRange      range;
    
    // id's for consisteny
    int         region_id      = -1;
    int         track_id       = -1;
    int         sub_group_id   = -1;
    
    // gain per channel
    std::vector<double> gain_vector;
};

inline void to_json(json& j, const AudioRegionData& r) {
    j = json{   {"name", r.name},
                {"start", r.range.getStart()},
                {"end", r.range.getEnd()},
                {"id", r.region_id},
                {"track_id", r.track_id},
                {"sub_group_id", r.sub_group_id},
                {"gain_vector", r.gain_vector},
    };
}

inline void from_json(const json& j, AudioRegionData& r) {
    j.at("name").get_to(r.name);
    r.range.setStart(j.at("start").get<double>());
    r.range.setEnd(j.at("end").get<double>());
    
    if (j.contains("id"))
        j.at("id").get_to(r.region_id);
    
    if (j.contains("track_id"))
        j.at("track_id").get_to(r.track_id);
    
    if (j.contains("sub_group_id"))
        j.at("sub_group_id").get_to(r.sub_group_id);
    
    if (j.contains("gain_vector"))
        j.at("gain_vector").get_to(r.gain_vector);
}
