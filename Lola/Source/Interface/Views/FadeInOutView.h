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
    FadeInOutView() = default;
    ~FadeInOutView() override = default;

    void paint (juce::Graphics&) override;
    void resized() override;
    
    void setPlayListItem(std::shared_ptr<PlayListItem> item);

private:
    
    std::shared_ptr<PlayListItem> playListItem;
    
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (FadeInOutView)
};
