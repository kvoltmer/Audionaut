/*
  ==============================================================================

    PlayListSchedulerData.h
    Created: 6 Mar 2024 4:59:16pm
    Author:  Klaus Voltmer

  ==============================================================================
*/

#pragma once

#include <JuceHeader.h>
#include <nlohmann/json.hpp>

using json = nlohmann::json;

//==============================================================================
struct PlayListSchedulerData
{
    // edit or arrangement mode
    bool editMode                   = false;
    
    // user interface follows transport
    bool followTransport            = true;
    
    // transport position in 96th clocks
    double transportPositionClocks  = 0.0;
    
    // transport start position
    double startPositionClocks      = 0.0;

};

NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE_WITH_DEFAULT(PlayListSchedulerData,
                                                editMode,
                                                followTransport,
                                                transportPositionClocks,
                                                startPositionClocks);

