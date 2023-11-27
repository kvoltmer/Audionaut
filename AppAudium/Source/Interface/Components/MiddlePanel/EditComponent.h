/*
  ==============================================================================

    EditComponent.h
    Created: 26 Nov 2023 5:37:41pm
    Author:  Klaus Voltmer

  ==============================================================================
*/

#pragma once

#include <JuceHeader.h>
#include "ArrangementEditBaseComponent.h"

class EditComponent  : public ArrangementEditBaseComponent
{
public:
    
    typedef ArrangementEditBaseComponent tBase;
    
    EditComponent(std::shared_ptr<AudiumEngine> audiumEngine) :
        tBase(audiumEngine, false)
    {
    }


private:
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (EditComponent)
};
