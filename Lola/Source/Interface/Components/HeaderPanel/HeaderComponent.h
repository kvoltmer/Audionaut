
#pragma once

#include <JuceHeader.h>

#include "Interface/Controls/LevelMeter.h"

class AudiumEngine;

class HeaderComponent  : public juce::Component,
                         private juce::Timer
{
public:
    HeaderComponent (std::shared_ptr<AudiumEngine> audiumEngine);
    ~HeaderComponent() override;


    void timerCallback() override;

    void paint (juce::Graphics& g) override;
    void resized() override;

    void updateUI();
    
    std::function<void()> onRightPanelButtonClick;
    
private:
    
    void configureSlider(juce::Slider* slider);
    
    std::shared_ptr<AudiumEngine> audiumEngine;

    std::unique_ptr<juce::TextButton> linkButton;
    std::unique_ptr<juce::Slider> tempoSlider;
    std::unique_ptr<juce::Slider> barsSlider;
    std::unique_ptr<juce::Slider> beatsSlider;
    std::unique_ptr<juce::Slider> clicksSlider;
    
    int lastBeatsValue = 0;
    int lastBarsValue = 0;
    int lastClicksValue = 0;
    
    std::unique_ptr<juce::DrawableButton> playButton;
    juce::DrawablePath playImage;
    
    std::unique_ptr<juce::DrawableButton> stopButton;
    juce::DrawablePath stopImage;
    
    std::unique_ptr<juce::ShapeButton> rightPanelButton;
    
    std::unique_ptr<StereoMeter> stereoMeter;
    std::unique_ptr<juce::Slider> volumeSlider;
    
    
    juce::Path getRightPanelButtonPath();

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (HeaderComponent)
};



