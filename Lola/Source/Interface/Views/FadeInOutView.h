/*
  ==============================================================================

    FadeInOutView.h
    Created: 8 Feb 2025 4:12:39pm
    Author:  Klaus Voltmer

  ==============================================================================
*/

#pragma once

#include <JuceHeader.h>

#include "FadeInOutView.h"

class PlayListItem;

//==============================================================================
/*
*/
class FadeInOutView  : public juce::Component
{
public:
    FadeInOutView(std::shared_ptr<PlayListItem> playListItem);
    ~FadeInOutView() override;

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    
    std::shared_ptr<PlayListItem> playListItem;
    
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (FadeInOutView)
};
