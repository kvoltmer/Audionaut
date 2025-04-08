//    Audionaut - Audio editing application for multitrack recordings.
//    Copyright (C) 2025 Klaus Voltmer
//
//    Audionaut uses a GPL/commercial licence - see LICENCE.md for details.

#pragma once

#include <JuceHeader.h>
#include "Interface/Components/MiddlePanel/ArrangementEditBaseComponent.h"

class EditComponent : public ArrangementEditBaseComponent
{
public:
        
    EditComponent(std::shared_ptr<audium::AudiumEngine> audiumEngine, std::shared_ptr<ZoomHandler> zoomHandler) :
        ArrangementEditBaseComponent(audiumEngine, zoomHandler, false)
    {
    }


private:
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (EditComponent)
};
