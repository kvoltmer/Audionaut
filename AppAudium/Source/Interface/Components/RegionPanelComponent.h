/*
  ==============================================================================

    RegionPanelComponent.h
    Created: 6 Jun 2023 11:50:49am
    Author:  Klaus Voltmer

  ==============================================================================
*/

#pragma once

#include <JuceHeader.h>

//==============================================================================
/*
*/
class RegionPanelComponent  : public juce::Component
{
public:
    RegionPanelComponent();
    ~RegionPanelComponent() override;

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (RegionPanelComponent)
};
