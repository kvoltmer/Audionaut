/*
  ==============================================================================

    FadeInOutView.h
    Created: 8 Feb 2025 4:12:39pm
    Author:  Klaus Voltmer

  ==============================================================================
*/

//    Audionaut - Audio editing application for multitrack recordings.
//    Copyright (C) 2025 Klaus Voltmer
//
//    Audionaut uses a GPL/commercial licence - see LICENCE.md for details.

#pragma once

#include <JuceHeader.h>

#include "FadeInOutView.h"
#include "Engine/PlayList/PlayListItem.h"


class FadeInOutView  : public juce::Component
{
public:
    FadeInOutView() = default;
    ~FadeInOutView() override = default;

    void paint (juce::Graphics&) override;
    void resized() override;
    
    void setPlayListItem(std::shared_ptr<audium::PlayListItem> item);

private:
    
    std::shared_ptr<audium::PlayListItem> playListItem;
    
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (FadeInOutView)
};
