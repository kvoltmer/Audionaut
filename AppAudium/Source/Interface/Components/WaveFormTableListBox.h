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
class WaveFormTableListBox  : public juce::TableListBox
{
public:
    WaveFormTableListBox (const juce::String& componentName = juce::String(),
                  juce::TableListBoxModel* model = nullptr);
    ~WaveFormTableListBox() override;
    
    void listWasScrolled() override;

private:
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (WaveFormTableListBox)
};
