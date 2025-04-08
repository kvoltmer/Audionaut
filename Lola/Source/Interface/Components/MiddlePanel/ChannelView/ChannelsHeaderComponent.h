//    Audionaut - Audio editing application for multitrack recordings.
//    Copyright (C) 2025 Klaus Voltmer
//
//    Audionaut uses a GPL/commercial licence - see LICENCE.md for details.

#pragma once

#include <JuceHeader.h>

#include "Engine/AudiumEngine.h"
#include "Engine/Resource/AudioResourceContainer.h"

class ChannelsHeaderComponent  : public juce::Component
{
public:
    ChannelsHeaderComponent(std::shared_ptr<audium::AudiumEngine> audiumEngine) :
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
    std::shared_ptr<audium::AudiumEngine> audiumEngine;
    
    
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ChannelsHeaderComponent)
};
