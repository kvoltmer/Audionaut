/*
  ==============================================================================

    RightPanelComponent.h
    Created: 6 Jun 2023 11:50:49am
    Author:  Klaus Voltmer

  ==============================================================================
*/

#pragma once

#include <JuceHeader.h>

//==============================================================================
/*
*/
class AudiumEngine;
class PlayListComponent;
class RegionComponent;

class RightPanelComponent  : public juce::Component
{
public:
    RightPanelComponent(std::shared_ptr<AudiumEngine> audiumEngine);
    ~RightPanelComponent() override;

    void paint (juce::Graphics&) override;
    void resized() override;
    void updateUI();
    void clearSelection();

private:
    std::shared_ptr<AudiumEngine> audiumEngine;

    std::unique_ptr<RegionComponent> regionComponent;
    std::unique_ptr<juce::StretchableLayoutManager> stretchableLayoutManager;
    std::unique_ptr<juce::StretchableLayoutResizerBar> stretchableLayoutResizerBar;
    std::unique_ptr<PlayListComponent> playListComponent;
    
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (RightPanelComponent)
};
