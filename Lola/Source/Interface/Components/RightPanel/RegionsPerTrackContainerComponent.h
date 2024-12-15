/*
  ==============================================================================

    RegionsPerTrackContainerComponent.h
    Created: 14 Dec 2024 10:31:03am
    Author:  Klaus Voltmer

  ==============================================================================
*/

#pragma once

#include <JuceHeader.h>
#include "Interface/UIContext.h"

class AudiumEngine;
class RegionsPerTrackComponent;
//==============================================================================
/*
*/
class RegionsPerTrackContainerComponent  : public juce::Component
{
public:
    RegionsPerTrackContainerComponent(std::shared_ptr<AudiumEngine> audiumEngine);

    ~RegionsPerTrackContainerComponent() override
    {
    }

    void paint (juce::Graphics& g) override
    {
    }

    void resized() override;
    
    void createComponents();

    void updateUI(UIContext context = RebuildContext);
    

private:
    
    std::shared_ptr<AudiumEngine> audiumEngine;
    std::vector<std::shared_ptr<RegionsPerTrackComponent>> regionsPerTrackComponents;

    
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (RegionsPerTrackContainerComponent)
};
