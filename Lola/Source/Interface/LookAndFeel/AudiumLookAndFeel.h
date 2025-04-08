//    Audionaut - Audio editing application for multitrack recordings.
//    Copyright (C) 2025 Klaus Voltmer
//
//    Audionaut uses a GPL/commercial licence - see LICENCE.md for details.

#pragma once

#include <JuceHeader.h>
#include "Interface/ColourIds.h"

class AudiumLookAndFeel : public juce::LookAndFeel_V4
{
public:
    AudiumLookAndFeel();
    ~AudiumLookAndFeel() override;
    
    static LookAndFeel_V4::ColourScheme getDarkAudiumColourScheme();
    
    // Button
    void drawButtonBackground (juce::Graphics& g,
                               juce::Button& button,
                               const juce::Colour& backgroundColour,
                               bool shouldDrawButtonAsHighlighted,
                               bool shouldDrawButtonAsDown) override;
    
    
    // Combobox
    juce::Font getComboBoxFont (juce::ComboBox&) override;
    void drawComboBox (juce::Graphics& g, int width, int height, bool,
                                          int, int, int, int, juce::ComboBox& box) override;
    void positionComboBoxText (juce::ComboBox& box, juce::Label& label) override;
    juce::Label* createComboBoxTextBox (juce::ComboBox&) override;
    juce::PopupMenu::Options getOptionsForComboBoxPopupMenu (juce::ComboBox& box, juce::Label& label) override;

    
    // AlertWindow
    int getAlertWindowButtonHeight() override;
    juce::Font getAlertWindowTitleFont() override;
    juce::Font getAlertWindowMessageFont() override;
    juce::Font getAlertWindowFont() override;
    
    
    // PopupMenu
    juce::Font getPopupMenuFont() override;
    
    // Slider
    juce::Label* createSliderTextBox (juce::Slider& slider) override;
    void drawLinearSlider (juce::Graphics& g, int x, int y, int width, int height,
                                           float sliderPos,
                                           float minSliderPos,
                                           float maxSliderPos,
                                           const juce::Slider::SliderStyle style, juce::Slider& slider) override;
    
    // Table
    virtual void drawTableHeaderColumn (juce::Graphics& g, juce::TableHeaderComponent& header,
                                                const juce::String& columnName, int /*columnId*/,
                                                int width, int height, bool isMouseOver, bool isMouseDown,
                                                int columnFlags) override;
    
    // Various statics
    static const int channelsWidth = 100;
    static const int dragZoomControlHeight = 25;
    static const int transportPositionControlHeight = 25;
    static const int tableHeaderHeight = 25;
    static const float defaultFontSize;
    static const int extraSpaceAtBottom = 200;
    static const int maxTrackColours = 256;
    
    static juce::Colour trackColours[maxTrackColours];
private:
    void setupColours();
};


