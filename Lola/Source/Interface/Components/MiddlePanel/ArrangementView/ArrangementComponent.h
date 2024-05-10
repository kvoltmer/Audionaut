/*
  ==============================================================================

    ArrangementComponent.h
    Created: 23 Oct 2023 12:01:31pm
    Author:  Klaus Voltmer

  ==============================================================================
*/

#pragma once

#include <JuceHeader.h>

#include "Interface/Components/MiddlePanel/ArrangementEditBaseComponent.h"

class ArrangementComponent  : public ArrangementEditBaseComponent
{
public:
    
    typedef ArrangementEditBaseComponent tBase;
    
    ArrangementComponent(std::shared_ptr<AudiumEngine> audiumEngine, std::shared_ptr<ZoomHandler> zoomHandler) :
        tBase(audiumEngine, zoomHandler, true)
    {
    }

private:
    
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ArrangementComponent)
};
