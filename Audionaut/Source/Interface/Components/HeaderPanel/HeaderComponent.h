//    Audionaut - Audio editing application for multitrack recordings.
//    Copyright (C) 2025 Klaus Voltmer
//
//    Audionaut uses a GPL/commercial licence - see LICENCE.md for details.

#pragma once

#include <JuceHeader.h>

#include "Interface/Controls/LevelMeter.h"

#include "Engine/AudiumEngine.h"

class HeaderComponent  : public juce::Component,
                         private juce::Timer
{
public:
    HeaderComponent (std::shared_ptr<audium::AudiumEngine> audiumEngine);
    ~HeaderComponent() override;


    void timerCallback() override;

    void paint (juce::Graphics& g) override;
    void resized() override;

    void updateUI();
    
    std::function<void()> onRightPanelButtonClick;
    
private:
        
    std::shared_ptr<audium::AudiumEngine> audiumEngine;

    std::unique_ptr<juce::TextButton> linkButton;
    std::unique_ptr<juce::Slider> tempoSlider;
    std::unique_ptr<juce::Slider> barsSlider;
    std::unique_ptr<juce::Slider> beatsSlider;
    std::unique_ptr<juce::Slider> clicksSlider;
    
    int lastBeatsValue = 0;
    int lastBarsValue = 0;
    int lastClicksValue = 0;
    
    std::unique_ptr<juce::DrawableButton> playButton;
    std::unique_ptr<juce::DrawableButton> stopButton;
    std::unique_ptr<juce::DrawableButton> recordButton;
    
    std::unique_ptr<juce::DrawableButton> loopButton;
    std::unique_ptr<juce::ShapeButton> rightPanelButton;
    
    std::unique_ptr<StereoMeter> stereoMeter;
    std::unique_ptr<juce::Slider> volumeSlider;
    
    
    juce::Path getRightPanelButtonPath();
    juce::Path getLoopButtonPath();

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (HeaderComponent)
};



