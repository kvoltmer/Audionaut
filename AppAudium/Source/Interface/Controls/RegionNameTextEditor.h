/*
  ==============================================================================

    RegionNameTextEditor.h
    Created: 8 Jun 2023 5:45:45pm
    Author:  Klaus Voltmer

  ==============================================================================
*/

#pragma once

#include <JuceHeader.h>
#include "Engine/AudioRegion.h"

//==============================================================================
/*
*/
class RegionNameTextEditor  : public juce::TextEditor
{
public:
    RegionNameTextEditor(std::shared_ptr<AudioRegion> audioRegion) :
        audioRegion(audioRegion)
    {
        // In your constructor, you should add any child components, and
        // initialise any special settings that your component needs.
        setMultiLine (false);
        setReturnKeyStartsNewLine (false);
        setReadOnly (false);
        setScrollbarsShown (true);
        setCaretVisible (true);
        setPopupMenuEnabled (true);
        setText (audioRegion->name);
    }

    ~RegionNameTextEditor() override
    {
    }

    void paint (juce::Graphics& g) override
    {
        /* This demo code just fills the component's background and
           draws some placeholder text to get you started.

           You should replace everything in this method with your own
           drawing code..
        */

//        g.fillAll (getLookAndFeel().findColour (juce::ResizableWindow::backgroundColourId));   // clear the background
//
//        g.setColour (juce::Colours::grey);
//        g.drawRect (getLocalBounds(), 1);   // draw an outline around the component
//
//        g.setColour (juce::Colours::white);
//        g.setFont (14.0f);
//        g.drawText ("RegionNameTextEditor", getLocalBounds(),
//                    juce::Justification::centred, true);   // draw some placeholder text
    }

    void resized() override
    {
        // This method is where you should set the bounds of any child
        // components that your component contains..

    }

private:
    std::shared_ptr<AudioRegion> audioRegion;
    
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (RegionNameTextEditor)
};
