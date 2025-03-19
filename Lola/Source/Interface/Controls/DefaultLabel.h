//    Lola - Audio editing application for multitrack recordings.
//    Copyright (C) 2025 Klaus Voltmer
//
//    Lola uses a GPL/commercial licence - see LICENCE.md for details.


#pragma once

#include <JuceHeader.h>
#include "Interface/LookAndFeel/AudiumLookAndFeel.h"

class DefaultLabel  : public juce::Label
{
public:
    DefaultLabel (const juce::String& componentName = juce::String(),
                 const juce::String& labelText = juce::String()) :
        juce::Label(componentName, labelText)
    {
        setFont (juce::FontOptions (AudiumLookAndFeel::defaultFontSize));
        setJustificationType (juce::Justification::left);
        setEditable (true, true, false);
        
        setColour (juce::Label::backgroundColourId, juce::Colours::grey);
        setColour (juce::TextEditor::highlightColourId, juce::Colours::lightgrey);
    }

    ~DefaultLabel() override
    {
    }

private:
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (DefaultLabel)
};
