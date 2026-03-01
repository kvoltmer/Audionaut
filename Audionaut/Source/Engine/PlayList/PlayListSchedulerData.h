//    Audionaut - Audio editing application for multitrack recordings.
//    Copyright (C) 2025 Klaus Voltmer
//
//    Audionaut uses a GPL/commercial licence - see LICENCE.md for details.

#pragma once

#include <JuceHeader.h>
#include <nlohmann/json.hpp>

using json = nlohmann::json;

namespace audium {

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
    
    // armed for recording or not
    bool isRecordingArmed = false;
    
    // recording or not
    bool isRecording = false;
    
    // the position where the recording was started
    double recordingStartPositionClocks = 0.0;
    
};

NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE_WITH_DEFAULT(PlayListSchedulerData,
                                                editMode,
                                                followTransport,
                                                transportPositionClocks,
                                                startPositionClocks);

}

