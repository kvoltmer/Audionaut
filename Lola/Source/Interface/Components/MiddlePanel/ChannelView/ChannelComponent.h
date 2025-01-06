/*
  ==============================================================================

  This is an automatically generated GUI class created by the Projucer!

  Be careful when adding custom code to these files, as only the code within
  the "//[xyz]" and "//[/xyz]" sections will be retained when the file is loaded
  and re-saved.

  Created with Projucer version: 7.0.8

  ------------------------------------------------------------------------------

  The Projucer is part of the JUCE library.
  Copyright (c) 2020 - Raw Material Software Limited.

  ==============================================================================
*/

#pragma once

#include <JuceHeader.h>

#include "Engine/Resource/AudioResource.h"
#include "Engine/AudiumEngine.h"

#include "Interface/Controls/LevelMeter.h"

class ChannelComponent  : public juce::Component,
                          private juce::Timer,
                          public juce::ComboBox::Listener,
                          public juce::Button::Listener,
                          public juce::Label::Listener
{
public:
    ChannelComponent (std::shared_ptr<AudioTrack> audioTrack,
                      std::shared_ptr<AudiumEngine> engine,
                      int rowNumber);
    ~ChannelComponent() override;


    void refreshComponent(std::shared_ptr<AudioTrack> audioTrack, int rowNumber, bool isRowSelected);
    void timerCallback() override;
    void stopTheTimer() { stopTimer(); }
    void comboBoxChanged (juce::ComboBox* comboBoxThatHasChanged) override;

    void paint (juce::Graphics& g) override;
    void resized() override;
    void buttonClicked (juce::Button* buttonThatWasClicked) override;
    void labelTextChanged (juce::Label* labelThatHasChanged) override;
    void mouseDown (const juce::MouseEvent& e) override;
    void mouseUp (const juce::MouseEvent& e) override;
    bool keyPressed (const juce::KeyPress& key) override;

    // Binary resources:
    static const char* channelScale_png;
    static const int channelScale_pngSize;

    enum { moveChannelToNewTrackId = 0xf836743, reservedId = 0xf836744 };

    std::shared_ptr<AudiumEngine> getEngine() const { return engine; }
    
private:
    std::shared_ptr<AudioTrack> audioTrack;
    std::shared_ptr<AudiumEngine> engine;
    std::unique_ptr<LevelMeter> levelMeter;
    std::unique_ptr<juce::ComboBox> channelSizeComboBox;
    std::unique_ptr<juce::Slider> volumeSlider;
    std::unique_ptr<juce::Slider> panSlider;
    std::unique_ptr<juce::ImageButton> volumeScaleButton;

    int rowNumber = 0;
    
    static void configureVolumeSlider(juce::Slider *slider);
    static void configurePanSlider(juce::Slider *slider);
    
    // linear scaling
    static const double scale_linear(const double dVal, const double dMin, const double dMax)
    {
        return dMin + (dVal * abs(dMax - dMin));
    }

    // reverse linear scaling
    static double reverse_linear(const double dVal, const double dMin, const double dMax)
    {
        return abs(dVal - dMin) / abs(dMax - dMin);
    }
    
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ChannelComponent)
};

