//    Audionaut - Audio editing application for multitrack recordings.
//    Copyright (C) 2025 Klaus Voltmer
//
//    Audionaut uses a GPL/commercial licence - see LICENCE.md for details.

#pragma once

#include <JuceHeader.h>

#include "Engine/AudiumEngine.h"
#include "Interface/ColourIds.h"

/**
 * The mixer at the bottom of the middle panel: one strip per channel,
 * grouped by track, followed by the aux buses and the master.
 *
 * Still the frame only: the strips come with the next steps of phase 5 of
 * the effects plan.
 */
class MixerComponent : public juce::Component
{
public:
    explicit MixerComponent (std::shared_ptr<audium::AudiumEngine> engine) :
        audiumEngine (std::move (engine))
    {
    }

    void paint (juce::Graphics& g) override
    {
        g.fillAll (findColour (audium::secondaryBackgroundColourId));
    }

    void resized() override
    {
    }

    /// Re-reads the engine: tracks, channels and their strips.
    void updateUI()
    {
    }

private:
    std::shared_ptr<audium::AudiumEngine> audiumEngine;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (MixerComponent)
};
