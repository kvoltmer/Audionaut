//    Audionaut - Audio editing application for multitrack recordings.
//    Copyright (C) 2025 Klaus Voltmer
//
//    Audionaut uses a GPL/commercial licence - see LICENCE.md for details.

#pragma once

#include <JuceHeader.h>
#include "Interface/UIContext.h"

#include "Engine/AudiumEngine.h"

class PlayListComponent;

class PlayListContainerComponent  : public juce::Component, private juce::Timer
{
public:
    PlayListContainerComponent(std::shared_ptr<audium::AudiumEngine> audiumEngine);
    ~PlayListContainerComponent() override;

    void paint (juce::Graphics&) override;
    void resized() override;
    
    void createComponents();

    void updateUI(UIContext context = RebuildContext);
    
private:
    
    void timerCallback() override;
    
    std::shared_ptr<audium::AudiumEngine> audiumEngine;
    std::vector<std::shared_ptr<PlayListComponent>> playListComponents;
    
    std::unique_ptr<juce::Label> totalLengthLabel;
    std::unique_ptr<juce::Label> numVoicesLabel;
    
    
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (PlayListContainerComponent)
};
