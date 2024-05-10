/*
  ==============================================================================

    EditComponent.h
    Created: 26 Nov 2023 5:37:41pm
    Author:  Klaus Voltmer

  ==============================================================================
*/

#pragma once

#include <JuceHeader.h>
#include "Interface/Components/MiddlePanel/ArrangementEditBaseComponent.h"

class EditComponent : public ArrangementEditBaseComponent
{
public:
        
    EditComponent(std::shared_ptr<AudiumEngine> audiumEngine, std::shared_ptr<ZoomHandler> zoomHandler) :
        ArrangementEditBaseComponent(audiumEngine, zoomHandler, false)
    {
    }


private:
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (EditComponent)
};
