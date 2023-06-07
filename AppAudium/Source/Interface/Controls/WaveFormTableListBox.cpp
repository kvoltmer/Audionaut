/*
  ==============================================================================

    WaveFormTableListBox.cpp
    Created: 16 Mar 2023 4:43:23pm
    Author:  Klaus Voltmer

  ==============================================================================
*/

#include <JuceHeader.h>
#include "WaveFormTableListBox.h"

//==============================================================================
WaveFormTableListBox::WaveFormTableListBox (const juce::String& componentName,
                                            audium::ListBoxModel* model) :
    audium::ListBox(componentName, model)
{
}

WaveFormTableListBox::~WaveFormTableListBox()
{
}

