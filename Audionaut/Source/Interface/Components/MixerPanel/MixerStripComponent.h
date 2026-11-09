//    Audionaut - Audio editing application for multitrack recordings.
//    Copyright (C) 2025 Klaus Voltmer
//
//    Audionaut uses a GPL/commercial licence - see LICENCE.md for details.

#pragma once

#include <JuceHeader.h>

#include "Interface/Components/MixerPanel/MixerStripElements.h"

/**
 * One bus channel as a mixer strip, laid out like a Pro Tools channel strip:
 * the elements of MixerStripElements.h stacked top to bottom (sends, pan,
 * record / monitor / solo / mute, fader with meter) and the name bar at the
 * bottom, each as tall as MixerLayout says.
 * The section bar's mask decides which of them show; the fader and the name
 * always do. The channel's I/O rows and the track's inserts are above the
 * strips, laid out by MixerTrackGroupComponent. refresh() re-reads the
 * channel into every element, refreshLive() runs the meter.
 */
class MixerStripComponent : public juce::Component
{
public:
    MixerStripComponent (MixerContext context, std::shared_ptr<audium::AudioTrack> track, int channelNumber);
    ~MixerStripComponent() override;

    /// Which sections to show (MixerLayout::Section flags).
    void setSections (int sections);

    /// Re-reads the channel: values, sends, selection, routing.
    void refresh();

    /// The level meter, on the mixer's timer.
    void refreshLive();

    void resized() override;
    void paint (juce::Graphics& g) override;

private:
    bool shows (MixerLayout::Section section) const noexcept { return (sections & section) != 0; }

    StripChannel channel;
    int sections = MixerLayout::allSections;

    std::unique_ptr<StripSends> sends;
    std::unique_ptr<StripPan> pan;
    std::unique_ptr<StripButtons> buttons;
    std::unique_ptr<StripFader> fader;
    std::unique_ptr<StripName> name;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (MixerStripComponent)
};
