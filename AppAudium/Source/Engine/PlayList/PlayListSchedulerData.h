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

