//    Audionaut - Audio editing application for multitrack recordings.
//    Copyright (C) 2025 Klaus Voltmer
//
//    Audionaut uses a GPL/commercial licence - see LICENCE.md for details.

#pragma once

#include <JuceHeader.h>
#include "Interface/UIContext.h"
#include "Engine/AudiumEngine.h"

class PlayListContainerComponent;
class RegionComponent;
class RegionContainerComponent;

class RightPanelComponent  : public juce::Component
{
public:
    RightPanelComponent(std::shared_ptr<audium::AudiumEngine> audiumEngine);
    ~RightPanelComponent() override;
    
    void paint (juce::Graphics&) override;
    void resized() override;
    void updateUI(UIContext context = ContentContext);
    
private:
    std::shared_ptr<audium::AudiumEngine> audiumEngine;

    std::unique_ptr<RegionComponent> regionComponent;
    std::unique_ptr<juce::StretchableLayoutManager> stretchableLayoutManager;
    std::unique_ptr<juce::StretchableLayoutResizerBar> stretchableLayoutResizerBar;
    std::unique_ptr<PlayListContainerComponent> playListContainerComponent;
    std::unique_ptr<RegionContainerComponent> regionContainerComponent;
    
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (RightPanelComponent)
};
