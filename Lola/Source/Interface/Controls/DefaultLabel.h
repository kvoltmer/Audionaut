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
