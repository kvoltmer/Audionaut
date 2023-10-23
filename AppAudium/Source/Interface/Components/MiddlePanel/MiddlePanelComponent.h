/*
  ==============================================================================

    MiddlePanelComponent.h
    Created: 6 Jun 2023 11:51:48am
    Author:  Klaus Voltmer

  ==============================================================================
*/

#pragma once

#include <JuceHeader.h>

#include "ChannelsComponent.h"
#include "ArrangementComponent.h"

class AudiumEngine;

class MiddlePanelComponent :    public juce::Component
{
public:
    MiddlePanelComponent(std::shared_ptr<AudiumEngine> audiumEngine)
    {
        channelsComponent.reset(new ChannelsComponent());
        addAndMakeVisible(channelsComponent.get());
        
        arrangementComponent.reset(new ArrangementComponent(audiumEngine));
        addAndMakeVisible(arrangementComponent.get());
    }
    
    ~MiddlePanelComponent() override
    {
    }
    
    void resized() override
    {
        auto channelsWidth = 100;
        
        channelsComponent->setBounds(getLocalBounds().removeFromLeft(channelsWidth));
        arrangementComponent->setBounds(getLocalBounds().removeFromRight(getWidth() - channelsWidth));
    }
    
    void updateUI()
    {
        arrangementComponent->updateUI();
    }
    
    void zoomIn()
    {
        arrangementComponent->zoomIn();
    }
    
    void zoomOut()
    {
        arrangementComponent->zoomOut();
    }
    
private:
    
    std::unique_ptr<ChannelsComponent> channelsComponent;
    std::unique_ptr<ArrangementComponent> arrangementComponent;
    
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (MiddlePanelComponent)
};
