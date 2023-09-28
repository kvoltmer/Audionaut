
#include <JuceHeader.h>
#include "AudioGroupListBox.h"

//==============================================================================
AudioGroupListBox::AudioGroupListBox (const juce::String& componentName,
                                            audium::ListBoxModel* model) :
    audium::ListBox(componentName, model)
{
}

AudioGroupListBox::~AudioGroupListBox()
{
}

