/*
  ==============================================================================

    ChannelsHeaderComponent.h
    Created: 15 Dec 2023 11:00:09am
    Author:  Klaus Voltmer

  ==============================================================================
*/

#pragma once

#include <JuceHeader.h>

#include "Engine/AudiumEngine.h"
#include "Engine/AudioResourceContainer.h"

class ChannelsHeaderComponent  : public juce::Component
{
public:
    ChannelsHeaderComponent(std::shared_ptr<AudiumEngine> audiumEngine) :
        audiumEngine(audiumEngine)
    {
    }

    ~ChannelsHeaderComponent() override
    {
    }

    void paint (juce::Graphics& g) override
    {
        g.fillAll (juce::Colour (0xff323232));
        
        
    }

    void resized() override
    {
    }
    

private:
    std::shared_ptr<AudiumEngine> audiumEngine;
    
    
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ChannelsHeaderComponent)
};
