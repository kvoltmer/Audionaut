/*
  ==============================================================================

    RegionContainerComponent.h
    Created: 12 Feb 2025 5:04:25pm
    Author:  Klaus Voltmer

  ==============================================================================
*/

#pragma once

#include <JuceHeader.h>
#include "Interface/UIContext.h"

class AudiumEngine;
class TrackRegionTableListBoxModel;

class RegionContainerComponent  : public juce::Component
{
public:
    RegionContainerComponent(std::shared_ptr<AudiumEngine> audiumEngine);
    ~RegionContainerComponent() override;

    void paint (juce::Graphics&) override;
    void resized() override;

    void updateUI(UIContext context = RebuildContext);
    
private:
    
    std::shared_ptr<AudiumEngine> audiumEngine;
    
    std::shared_ptr<juce::TableListBox> regionTableListBox;
    std::unique_ptr<TrackRegionTableListBoxModel> regionTableListBoxModel;
    
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (RegionContainerComponent)
};
