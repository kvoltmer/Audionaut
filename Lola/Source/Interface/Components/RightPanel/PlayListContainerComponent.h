/*
  ==============================================================================

    PlayListContainerComponent.h
    Created: 10 Oct 2023 10:33:05am
    Author:  Klaus Voltmer

  ==============================================================================
*/

#pragma once

#include <JuceHeader.h>
#include "Interface/UIContext.h"

class AudiumEngine;
class PlayListComponent;

class PlayListContainerComponent  : public juce::Component, private juce::Timer
{
public:
    PlayListContainerComponent(std::shared_ptr<AudiumEngine> audiumEngine);
    ~PlayListContainerComponent() override;

    void paint (juce::Graphics&) override;
    void resized() override;
    
    void createComponents();

    void updateUI(UIContext context = RebuildContext);
    
private:
    
    void timerCallback() override;
    
    std::shared_ptr<AudiumEngine> audiumEngine;
    std::vector<std::shared_ptr<PlayListComponent>> playListComponents;
    
    std::unique_ptr<juce::Label> totalLengthLabel;
    std::unique_ptr<juce::Label> numVoicesLabel;
    
    
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (PlayListContainerComponent)
};
