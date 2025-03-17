//    Lola - Audio editing application for multitrack recordings.
//    Copyright (C) 2025 Klaus Voltmer
//
//    This program is free software: you can redistribute it and/or modify
//    it under the terms of the GNU General Public License as published by
//    the Free Software Foundation, either version 3 of the License, or
//    (at your option) any later version.
//
//    This program is distributed in the hope that it will be useful,
//    but WITHOUT ANY WARRANTY; without even the implied warranty of
//    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
//    GNU General Public License for more details.
//
//    You should have received a copy of the GNU General Public License
//    along with this program.  If not, see <https://www.gnu.org/licenses/>.

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
    
    void configureSlider(juce::Slider* slider);
    
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
    std::unique_ptr<juce::DrawableButton> loopButton;
    std::unique_ptr<juce::ShapeButton> rightPanelButton;
    
    std::unique_ptr<StereoMeter> stereoMeter;
    std::unique_ptr<juce::Slider> volumeSlider;
    
    
    juce::Path getRightPanelButtonPath();
    juce::Path getLoopButtonPath();

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (HeaderComponent)
};



