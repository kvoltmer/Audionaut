//    Lola - Audio editing application for multitrack recordings.
//    Copyright (C) 2025 Klaus Voltmer
//
//    Lola uses a GPL/commercial licence - see LICENCE.md for details.

#pragma once

#include <JuceHeader.h>

#include "Interface/Components/MiddlePanel/ArrangementEditBaseComponent.h"

class ArrangementComponent  : public ArrangementEditBaseComponent
{
public:
    
    typedef ArrangementEditBaseComponent tBase;
    
    ArrangementComponent(std::shared_ptr<audium::AudiumEngine> audiumEngine, std::shared_ptr<ZoomHandler> zoomHandler) :
        tBase(audiumEngine, zoomHandler, true)
    {
    }

private:
    
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ArrangementComponent)
};
