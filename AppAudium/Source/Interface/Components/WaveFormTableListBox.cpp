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
              juce::TableListBoxModel* model) :
    juce::TableListBox(componentName, model)
{
    
}

WaveFormTableListBox::~WaveFormTableListBox()
{
}

void WaveFormTableListBox::listWasScrolled()
{
}

