
#pragma once

#include <JuceHeader.h>
#include "Interface/Widgets/audium_ListBox.h"

//==============================================================================
/*
*/
class AudioGroupListBox  : public audium::ListBox
{
public:
    AudioGroupListBox (const juce::String& componentName = juce::String(),
                          audium::ListBoxModel* model = nullptr);
    ~AudioGroupListBox() override;
    
private:
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (AudioGroupListBox)
};
