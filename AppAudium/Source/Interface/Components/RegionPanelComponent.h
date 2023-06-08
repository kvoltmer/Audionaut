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
class RegionTableListBox;
class RegionTableListBoxModel;
class AudiumEngine;

class RegionPanelComponent  : public juce::Component
{
public:
    RegionPanelComponent(std::shared_ptr<AudiumEngine> audiumEngine);
    ~RegionPanelComponent() override;

    void paint (juce::Graphics&) override;
    void resized() override;
    void updateUI();
    void clearSelection();

private:
    
    std::shared_ptr<RegionTableListBox> regionTableListBox;
    std::shared_ptr<RegionTableListBoxModel> regionTableListBoxModel;
    std::shared_ptr<AudiumEngine> audiumEngine;
    
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (RegionPanelComponent)
};
