//    Audionaut - Audio editing application for multitrack recordings.
//    Copyright (C) 2025 Klaus Voltmer
//
//    Audionaut uses a GPL/commercial licence - see LICENCE.md for details.

#pragma once

#include <JuceHeader.h>

#include "Interface/Controls/LevelMeter.h"
#include "Interface/Controls/LoadMeter.h"
#include "Interface/Controls/PanelToggleButton.h"

#include "Engine/AudiumEngine.h"

class HeaderComponent  : public juce::Component,
                         private juce::Timer
{
public:
    HeaderComponent (std::shared_ptr<audium::AudiumEngine> audiumEngine);
    ~HeaderComponent() override;

    static constexpr int preferredHeight = 40;


    void timerCallback() override;

    void paint (juce::Graphics& g) override;
    void resized() override;

    void updateUI();
    
    std::function<void()> onRightPanelButtonClick;
    std::function<void()> onMixerButtonClick;

    /// The panel toggles show which panels are open.
    void setPanelStates (bool rightPanelShown, bool mixerShown);

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
    std::unique_ptr<PanelToggleButton> rightPanelButton;
    std::unique_ptr<PanelToggleButton> mixerButton;

    std::unique_ptr<StereoMeter> stereoMeter;
    std::unique_ptr<LoadMeter> loadMeter;
    std::unique_ptr<juce::Slider> volumeSlider;
    
    
    /// A bars/beats/clicks slider: dragging it moves the play position by
    /// the dragged number of units.
    std::unique_ptr<juce::Slider> makePositionSlider (const juce::String& name,
                                                      double maximum,
                                                      int& lastValue,
                                                      double (*unitsToClocks) (double),
                                                      std::function<juce::String (double)> textFromValue);

    juce::Path getLoopButtonPath();

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (HeaderComponent)
};



