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

    // Typeface — routes every juce::Font without an explicit typeface to the
    // embedded Inter faces, so the UI renders identically on all platforms
    // instead of falling through to the per-platform default sans-serif.
    juce::Typeface::Ptr getTypefaceForFont (const juce::Font& font) override;

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
    
    static void configureSlider(juce::Slider* slider)
    {
        slider->setSliderStyle(juce::Slider::LinearBarVertical);
        slider->setVelocityBasedMode(true);
        slider->setDoubleClickReturnValue(true, 0.0);
        slider->setColour(juce::Slider::trackColourId, juce::Colours::transparentBlack);
        slider->setColour (juce::Slider::backgroundColourId, juce::Colours::grey);
    }
    
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
    
    static void configureVolumeSlider(juce::Slider *slider, double dbMax = 6.0)
    {
        slider->setSliderStyle(juce::Slider::LinearBarVertical);
        slider->setColour(juce::Slider::textBoxTextColourId, juce::Colours::white);
        slider->setColour(juce::Slider::trackColourId, juce::Colours::transparentBlack);
        
        slider->setTextValueSuffix (" dB");
        slider->setNumDecimalPlacesToDisplay(1);
        slider->setDoubleClickReturnValue(true, 0.0);
        slider->setVelocityModeParameters(1.0, 1, 0.05);
        slider->setVelocityBasedMode(true);
        
        auto scaled2UnscaledFunc = [](auto min, auto max, auto scaled) {
            return scale_linear(pow(scaled, 0.33333333), min, max);
        };
        auto unscaled2ScaledFunc = [](auto min, auto max, auto unscaled) {
            return pow(reverse_linear(unscaled, min, max), 3.0);
        };
        slider->setNormalisableRange(juce::NormalisableRange<double>(-80.0, dbMax,
                                                               scaled2UnscaledFunc,
                                                               unscaled2ScaledFunc));
        
        slider->textFromValueFunction = [](auto val) {
            if (val <= -80.0)
                return juce::String("-") + juce::String(juce::CharPointer_UTF8 ("\xe2\x88\x9e"));
            return juce::String(val, 1);
        };
        slider->updateText();
    }
    
    // Table
    virtual void drawTableHeaderColumn (juce::Graphics& g, juce::TableHeaderComponent& header,
                                                const juce::String& columnName, int /*columnId*/,
                                                int width, int height, bool isMouseOver, bool isMouseDown,
                                                int columnFlags) override;

    // Header text is sized from the header height. Shared so this and the
    // TrackColourLookAndFeel override cannot drift apart.
    static juce::FontOptions getTableHeaderFontOptions (int height)
    {
        return juce::FontOptions ((float) height * 0.5f, juce::Font::bold);
    }

    // Tabular figures, for readouts whose digits change while the user watches
    // them. Inter's proportional digits differ in advance by enough to shift the
    // surrounding text as a value ticks over - a ten-digit string spans 43.7px of
    // ones but 66.5px of eights at 13pt - which reads as jitter. With "tnum"
    // every digit takes the widest advance, so the text stays put.
    static juce::FontOptions withTabularFigures (juce::FontOptions opts)
    {
        return opts.withFeatureEnabled (juce::FontFeatureTag ("tnum"));
    }
    
    // Various statics
    static const int channelsWidth = 140;
    static const int standardItemHeight = 25;
    static const int popupMenuItemHeight = 20;
    static const int dragZoomControlHeight = standardItemHeight;
    static const int transportPositionControlHeight = standardItemHeight;
    static const int tableHeaderHeight = standardItemHeight;
    static const float defaultFontSize;
    static const int extraSpaceAtBottom = 200;
    static const int maxTrackColours = 256;
    
    static const int timerHz = 60;
    
    static juce::Colour trackColours[maxTrackColours];
    
    
    enum SizeIds
    {
        micro   = 25,
        small   = 50,
        medium  = 100,
        large   = 200,
        huge    = 400,
    };
    
private:
    void setupColours();
    void setupTypefaces();

    juce::Typeface::Ptr interRegular, interBold, interItalic, interBoldItalic;
};


