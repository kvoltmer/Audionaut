//    Audionaut - Audio editing application for multitrack recordings.
//    Copyright (C) 2025 Klaus Voltmer
//
//    Audionaut uses a GPL/commercial licence - see LICENCE.md for details.

#pragma once

#include <JuceHeader.h>

#include "Interface/Components/MixerPanel/MixerStripComponent.h"
#include "Interface/Components/MixerPanel/InsertSlotListComponent.h"

/**
 * One track in the mixer: a band in the track's colour with its name, each
 * channel's input and output under it in its strip's column, the track's
 * insert slots across the group's width, and one strip per channel below.
 * Inserts are per track (they process all of the track's channels), so they
 * appear once here and not in the strips; the I/O rows stand above them
 * because the routing comes first in the signal's order. The arrangement's minimized state does not apply to
 * the mixer.
 */
class MixerTrackGroupComponent : public juce::Component
{
public:
    MixerTrackGroupComponent (MixerContext context, std::shared_ptr<audium::AudioTrack> track);
    ~MixerTrackGroupComponent() override;

    int getPreferredWidth() const;

    /// How far each section is open (MixerLayout::Shown), for the I/O rows, the inserts and the strips.
    void setShown (const MixerLayout::Shown& shown);
    void refresh();
    void refreshLive();

    void resized() override;
    void paint (juce::Graphics& g) override;

private:
    class Band;

    audium::InsertChainAddress chainAddress() const;
    juce::String editorTitle (int slotIndex) const;

    MixerContext context;
    std::shared_ptr<audium::AudioTrack> track;
    MixerLayout::Shown shown = MixerLayout::Shown::of (MixerLayout::allSections);

    std::unique_ptr<Band> band;
    std::vector<std::unique_ptr<StripIo>> ios;   ///< one per channel, in the strips' columns
    MixerLayout::SectionRoom ioRoom;               ///< holds the I/O rows: they open and close together
    std::unique_ptr<InsertSlotListComponent> inserts;
    MixerLayout::SectionRoom insertsRoom;
    std::vector<std::unique_ptr<MixerStripComponent>> strips;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (MixerTrackGroupComponent)
};
