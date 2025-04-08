//    Audionaut - Audio editing application for multitrack recordings.
//    Copyright (C) 2025 Klaus Voltmer
//
//    Audionaut uses a GPL/commercial licence - see LICENCE.md for details.

#pragma once

#include <JuceHeader.h>
#include <nlohmann/json.hpp>

using json = nlohmann::json;

namespace audium {

struct LoopData
{
    // loop start
    double loopStartPositionClocks  = 96.0;
    
    // loop end
    double loopEndPositionClocks    = 288.0;
    
    // the smallest possible loop length
    double minimumLoopLengthClocks  = 1.0;
    
    // loop active
    bool loopActive                 = false;
    
};

NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE_WITH_DEFAULT(LoopData,
                                                loopStartPositionClocks,
                                                loopEndPositionClocks,
                                                loopActive);

} // namespace audium
