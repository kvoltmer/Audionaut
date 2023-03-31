/*
  ==============================================================================

    WaveFormTableListBox.h
    Created: 16 Mar 2023 4:43:23pm
    Author:  Klaus Voltmer

  ==============================================================================
*/

#pragma once

#include <JuceHeader.h>

//==============================================================================
/*
*/
class WaveFormTableListBox  : public juce::ListBox
{
public:
    WaveFormTableListBox (const juce::String& componentName = juce::String(),
                  juce::ListBoxModel* model = nullptr);
    ~WaveFormTableListBox() override;

private:
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (WaveFormTableListBox)
};
