//    Audionaut - Audio editing application for multitrack recordings.
//    Copyright (C) 2025 Klaus Voltmer
//
//    Audionaut uses a GPL/commercial licence - see LICENCE.md for details.

#pragma once

#include <JuceHeader.h>

class AudiumLabel  : public juce::Label
{
public:
    AudiumLabel (const juce::String& componentName = juce::String(),
           const juce::String& labelText = juce::String()) :
        juce::Label(componentName, labelText)
    {
        setEditable (false, true, true);
    }
    
    ~AudiumLabel() override
    {
    }
    
    
    
    /// pass on mouse events. unless row is not selected
    void mouseDown (const juce::MouseEvent& e) override
    {
        getParentComponent()->mouseDown(e);
    }
    
    void mouseDrag(const juce::MouseEvent& e) override
    {
        getParentComponent()->mouseDrag(e);
    }
};
