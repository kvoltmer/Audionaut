/*
  ==============================================================================

    ChannelsComponent.h
    Created: 23 Oct 2023 12:02:02pm
    Author:  Klaus Voltmer

  ==============================================================================
*/

#pragma once

#include <JuceHeader.h>

#include "Interface/ColourIds.h"

class ChannelsComponent  : public juce::Component
{
public:
    ChannelsComponent()
    {

    }

    ~ChannelsComponent() override
    {
    }

    void paint (juce::Graphics& g) override
    {
        g.fillAll(findColour(audium::secondaryBackgroundColourId));


//        g.fillAll (getLookAndFeel().findColour (juce::ResizableWindow::backgroundColourId));   // clear the background
//
//        g.setColour (juce::Colours::grey);
//        g.drawRect (getLocalBounds(), 1);   // draw an outline around the component
//
//        g.setColour (juce::Colours::white);
//        g.setFont (14.0f);
//        g.drawText ("ChannelsComponent", getLocalBounds(),
//                    juce::Justification::centred, true);   // draw some placeholder text
    }

    void resized() override
    {
        // This method is where you should set the bounds of any child
        // components that your component contains..

    }

private:
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ChannelsComponent)
};
