/*
  ==============================================================================

    WaveFormPanelComponent.h
    Created: 6 Jun 2023 11:51:48am
    Author:  Klaus Voltmer

  ==============================================================================
*/

#pragma once

#include <JuceHeader.h>

//==============================================================================
/*
*/

class ZoomHandler;
class RegionSelector;
class WaveFormTableListBox;
class WaveFormTableListBoxModel;
class AudiumEngine;

class WaveFormPanelComponent  : public juce::Component
{
public:
    WaveFormPanelComponent(std::shared_ptr<AudiumEngine> audiumEngine);
    ~WaveFormPanelComponent() override;

    void paint (juce::Graphics&) override;
    void resized() override;

    void updateUI();
    void zoomIn();
    void zoomOut();
    
private:
    
    std::shared_ptr<ZoomHandler> zoomHandler;
    std::shared_ptr<RegionSelector> regionSelector;
    std::shared_ptr<WaveFormTableListBox> waveFormTableListBox;
    std::shared_ptr<WaveFormTableListBoxModel> waveFormTableListBoxModel;
    std::shared_ptr<AudiumEngine> audiumEngine;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (WaveFormPanelComponent)
};
