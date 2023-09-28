/*
  ==============================================================================

    MiddlePanelComponent.h
    Created: 6 Jun 2023 11:51:48am
    Author:  Klaus Voltmer

  ==============================================================================
*/

#pragma once

#include <JuceHeader.h>

//==============================================================================
/*
*/

class AudiumEngine;
class ZoomHandler;
class RegionSelector;
class AudioGroupListBox;
class AudioGroupListBoxModel;
class PlayPositionMarker;

class MiddlePanelComponent  : public juce::Component
{
public:
    MiddlePanelComponent(std::shared_ptr<AudiumEngine> audiumEngine);
    ~MiddlePanelComponent() override;

    void paint (juce::Graphics&) override;
    void resized() override;

    void updateUI();
    void zoomIn();
    void zoomOut();
    
private:
    std::shared_ptr<AudiumEngine>               audiumEngine;
    std::shared_ptr<ZoomHandler>                zoomHandler;
    std::shared_ptr<RegionSelector>             regionSelector;
    std::shared_ptr<AudioGroupListBox>       audioGroupListBox;
    std::shared_ptr<AudioGroupListBoxModel>  audioGroupListBoxModel;
    std::shared_ptr<PlayPositionMarker>         playPositionMarker;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (MiddlePanelComponent)
};
