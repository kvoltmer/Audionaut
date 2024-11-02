/*
  ==============================================================================

    AudiumLookAndFeel.h
    Created: 7 Jun 2023 3:56:54pm
    Author:  Klaus Voltmer

  ==============================================================================
*/

#pragma once

#include <JuceHeader.h>
#include "ColourIds.h"

class AudiumLookAndFeel : public juce::LookAndFeel_V4
{
public:
    AudiumLookAndFeel();
    ~AudiumLookAndFeel() override;
    
    static LookAndFeel_V4::ColourScheme getDarkAudiumColourScheme();
    
    void drawButtonBackground (juce::Graphics& g,
                               juce::Button& button,
                               const juce::Colour& backgroundColour,
                               bool shouldDrawButtonAsHighlighted,
                               bool shouldDrawButtonAsDown) override;
    
    
    juce::Font getComboBoxFont (juce::ComboBox&) override;
    
    void drawComboBox (juce::Graphics& g, int width, int height, bool,
                                          int, int, int, int, juce::ComboBox& box) override;
    
    void positionComboBoxText (juce::ComboBox& box, juce::Label& label) override;
    
    juce::Label* createComboBoxTextBox (juce::ComboBox&) override;
    
    
    // AlertWindow
    int getAlertWindowButtonHeight() override;
    juce::Font getAlertWindowTitleFont() override;
    juce::Font getAlertWindowMessageFont() override;
    juce::Font getAlertWindowFont() override;
    
    static const int channelsWidth = 85;
    static const int dragZoomControlHeight = 25;
    static const int transportPositionControlHeight = 25;
    static const float labelFontSize;
    
private:
    void setupColours();
};


