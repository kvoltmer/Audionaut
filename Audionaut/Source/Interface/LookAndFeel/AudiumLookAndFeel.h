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

    /// A button as a key on a console: raised over a drop shadow (its parent paints that, see StripButtons) and
    /// pressed in while on or held down. Set the property on a button to draw it so.
    static constexpr const char* consoleButtonProperty = "consoleButton";
    static bool isConsoleButtonPressedIn (const juce::Button& button)
    {
        return button.getProperties().contains (consoleButtonProperty) && (button.getToggleState() || button.isDown());
    }


    
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
    
    // Resizer bar between two panels (the right panel's split): the same
    // seam line as PanelResizerBar draws for the main panels.
    void drawStretchableLayoutResizerBar (juce::Graphics& g, int w, int h, bool isVerticalBar,
                                          bool isMouseOver, bool isMouseDragging) override;

    // Slider
    juce::Label* createSliderTextBox (juce::Slider& slider) override;
    void drawRotarySlider (juce::Graphics& g, int x, int y, int width, int height, float sliderPosProportional,
                           float rotaryStartAngle, float rotaryEndAngle, juce::Slider& slider) override;

    /// A rotary slider's properties that shape its dial (see drawDial): where the value arc starts and
    /// where the mark sits, as proportions of the sweep; both 0.5 (the pan dial) when unset.
    static constexpr const char* dialArcFromProperty = "dialArcFrom";
    static constexpr const char* dialMarkProperty = "dialMark";

    /// The dials' sweep: from 7:30 over the top to 4:30, as JUCE measures angles (clockwise from 12).
    static constexpr float dialStartAngle = juce::MathConstants<float>::pi * 1.25f;
    static constexpr float dialEndAngle = juce::MathConstants<float>::pi * 2.75f;

    /// A dial as on a console, for the rotary sliders and anything that draws its own: a dark ring, a
    /// light domed face, a pointer at `proportion` of the sweep, the value as an arc from
    /// `arcFromProportion` to the pointer over a faint track, and a mark across the track at
    /// `markProportion` (negative: none). The pan dial arcs from the centre and marks it; a send dial
    /// arcs from the bottom and marks unity.
    static void drawDial (juce::Graphics& g, juce::Rectangle<float> area, float startAngle, float endAngle,
                          float proportion, float arcFromProportion, float markProportion,
                          bool enabled, bool active, juce::Colour indicator);

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
        
        slider->textFromValueFunction = decibelsText;
        slider->updateText();
    }

    /// A level as text: one decimal, "-inf" at the bottom of the range.
    static juce::String decibelsText (double decibels)
    {
        if (decibels <= -80.0)
            return juce::String ("-") + juce::String (juce::CharPointer_UTF8 ("\xe2\x88\x9e"));
        return juce::String (decibels, 1);
    }
    
    /// The mixer faders' taper, as on a console: +12 dB at the top, unity about three quarters of the
    /// way up, roughly 1 dB per 2 % of travel down to -20 dB, then compressing towards -inf (-80 dB)
    /// at the bottom. Linear between these anchors, in both directions.
    struct FaderTaper
    {
        static constexpr double minimumDecibels = -80.0;
        static constexpr double maximumDecibels = 12.0;

        struct Anchor { double decibels, position; };
        static constexpr Anchor anchors[] { { -80.0, 0.0 }, { -60.0, 0.10 }, { -40.0, 0.22 }, { -20.0, 0.42 }, { 0.0, 0.78 }, { 12.0, 1.0 } };

        /// 0 (bottom) to 1 (top).
        static double positionFor (double decibels)
        {
            decibels = juce::jlimit (minimumDecibels, maximumDecibels, decibels);
            for (size_t i = 1; i < std::size (anchors); ++i)
                if (decibels <= anchors[i].decibels)
                    return juce::jmap (decibels, anchors[i - 1].decibels, anchors[i].decibels, anchors[i - 1].position, anchors[i].position);
            return 1.0;
        }

        static double decibelsFor (double position)
        {
            position = juce::jlimit (0.0, 1.0, position);
            for (size_t i = 1; i < std::size (anchors); ++i)
                if (position <= anchors[i].position)
                    return juce::jmap (position, anchors[i - 1].position, anchors[i].position, anchors[i - 1].decibels, anchors[i].decibels);
            return maximumDecibels;
        }
    };

    /// A mixer fader: a vertical slider over -80 .. +12 dB on the FaderTaper, with the dB readout.
    static void configureFaderSlider (juce::Slider* slider)
    {
        slider->setSliderStyle (juce::Slider::LinearVertical);
        slider->setColour (juce::Slider::textBoxTextColourId, juce::Colours::white);
        slider->setTextValueSuffix (" dB");
        slider->setNumDecimalPlacesToDisplay (1);
        slider->setDoubleClickReturnValue (true, 0.0);
        slider->setNormalisableRange (juce::NormalisableRange<double> (FaderTaper::minimumDecibels, FaderTaper::maximumDecibels,
                                                                       [] (double, double, double position) { return FaderTaper::decibelsFor (position); },
                                                                       [] (double, double, double decibels) { return FaderTaper::positionFor (decibels); }));
        slider->textFromValueFunction = decibelsText;
        slider->updateText();
    }

    /// Where a level sits on the mixer faders' scale, 0 to 1 (the send rows' fill).
    static double levelPositionFor (double decibels)
    {
        return FaderTaper::positionFor (decibels);
    }

    // Table
    virtual void drawTableHeaderColumn (juce::Graphics& g, juce::TableHeaderComponent& header,
                                                const juce::String& columnName, int /*columnId*/,
                                                int width, int height, bool isMouseOver, bool isMouseDown,
                                                int columnFlags) override;

    // Header text is sized from the header height.
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


