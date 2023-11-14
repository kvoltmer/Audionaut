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

class MiddlePanelComponent : public juce::Component
{
public:
    MiddlePanelComponent(std::shared_ptr<AudiumEngine> audiumEngine)
    {
        channelsComponent.reset(new ChannelsComponent(audiumEngine));
        addAndMakeVisible(channelsComponent.get());
        
        arrangementComponent.reset(new ArrangementComponent(audiumEngine));
        addAndMakeVisible(arrangementComponent.get());
    }
    
    ~MiddlePanelComponent() override
    {
    }
    
    enum UIContext {
        EntireContext,
        ArrangementScrollContext
    };
    
    void resized() override
    {
        auto channelsWidth = 60;
        
        channelsComponent->setBounds(getLocalBounds().removeFromLeft(channelsWidth));
        arrangementComponent->setBounds(getLocalBounds().removeFromRight(getWidth() - channelsWidth));
    }
    
    void updateUI(UIContext context = EntireContext)
    {
        if (context == EntireContext)
        {
            arrangementComponent->updateUI();
            channelsComponent->updateUI();
        }
        else if(context == ArrangementScrollContext)
        {
            arrangementComponent->getRegionSelector()->updateFromEngine();
        }
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
