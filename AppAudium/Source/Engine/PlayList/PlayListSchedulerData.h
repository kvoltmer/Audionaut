/*
  ==============================================================================

    PlayListSchedulerData.h
    Created: 6 Mar 2024 4:59:16pm
    Author:  Klaus Voltmer

  ==============================================================================
*/

#pragma once

#include <nlohmann/json.hpp>

using json = nlohmann::json;

//==============================================================================
struct PlayListSchedulerData
{
    // Edit or Arrangement Mode
    bool editMode = true;
    
    bool followTransport = true;
    
    bool loopPlayList = false;
    
    // transport position in 96th clocks
    double transportPositionClocks = 0.0;
    
    double startPositionClocks = 0.0;
};

NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(PlayListSchedulerData, editMode, followTransport, loopPlayList, transportPositionClocks, startPositionClocks);

//
//inline void to_json(json& j, const PlayListSchedulerData& d) {
//    j = json{   {"edit_mode", d.editMode.load()},
//                {"follow_transport", d.followTransport.load()},
//                {"loop_playlist", d.loopPlayList.load()},
//                {"transport_position_clocks", d.transportPositionClocks.load()},
//                {"start_position_clocks", d.startPositionClocks.load()}
//    };
//}
//
//inline void from_json(const json& j, PlayListSchedulerData& d) {
//    d.editMode.store(j.at("edit_mode").get<bool>());
//    d.followTransport.store(j.at("follow_transport").get<bool>());
//    d.loopPlayList.store(j.at("loop_playlist").get<bool>());
//    d.transportPositionClocks.store(j.at("transport_position_clocks").get<bool>());
//    d.startPositionClocks.store(j.at("start_position_clocks").get<bool>());
//}
