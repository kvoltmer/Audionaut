/*
  ==============================================================================

    WaveFormTableListBox.h
    Created: 16 Mar 2023 4:43:23pm
    Author:  Klaus Voltmer

  ==============================================================================
*/

#pragma once

#include <JuceHeader.h>
#include "Interface/Widgets/audium_ListBox.h"

//==============================================================================
/*
*/
class WaveFormTableListBox  : public audium::ListBox
{
public:
    WaveFormTableListBox (const juce::String& componentName = juce::String(),
                          audium::ListBoxModel* model = nullptr);
    ~WaveFormTableListBox() override;
    
private:
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (WaveFormTableListBox)
};
