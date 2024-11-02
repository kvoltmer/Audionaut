/*
  ==============================================================================

    DefaultLabel.h
    Created: 1 Nov 2024 10:34:59am
    Author:  Klaus Voltmer

  ==============================================================================
*/

#pragma once

#include <JuceHeader.h>
#include "Interface/AudiumLookAndFeel.h"

class DefaultLabel  : public juce::Label
{
public:
    DefaultLabel (const juce::String& componentName = juce::String(),
                 const juce::String& labelText = juce::String()) :
        juce::Label(componentName, labelText)
    {
        setFont (juce::FontOptions (AudiumLookAndFeel::labelFontSize));
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
