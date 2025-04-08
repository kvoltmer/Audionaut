//    Audionaut - Audio editing application for multitrack recordings.
//    Copyright (C) 2025 Klaus Voltmer
//
//    Audionaut uses a GPL/commercial licence - see LICENCE.md for details.

#pragma once

#include <JuceHeader.h>
#include "Interface/UIContext.h"
#include "Interface/LookAndFeel/TrackColourLookAndFeel.h"

#include "Engine/AudiumEngine.h"

class TrackRegionTableListBoxModel;

class RegionContainerComponent  : public juce::Component
{
public:
    RegionContainerComponent(std::shared_ptr<audium::AudiumEngine> audiumEngine);
    ~RegionContainerComponent() override;

    void paint (juce::Graphics&) override;
    void resized() override;

    void updateUI(UIContext context = RebuildContext);
    
private:
    
    std::shared_ptr<audium::AudiumEngine> audiumEngine;
    
    TrackColourLookAndFeel lookAndFeel;
    
    std::shared_ptr<juce::TableListBox> regionTableListBox;
    std::unique_ptr<TrackRegionTableListBoxModel> regionTableListBoxModel;
    
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (RegionContainerComponent)
};
