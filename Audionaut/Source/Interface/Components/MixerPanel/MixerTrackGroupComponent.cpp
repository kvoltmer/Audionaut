//    Audionaut - Audio editing application for multitrack recordings.
//    Copyright (C) 2025 Klaus Voltmer
//
//    Audionaut uses a GPL/commercial licence - see LICENCE.md for details.

#include "MixerTrackGroupComponent.h"

#include "Interface/ColourIds.h"

using namespace audium;
using namespace MixerLayout;

//==============================================================================
/// The coloured band over a track's strips with the track's name.
class MixerTrackGroupComponent::Band : public juce::Component
{
public:
    explicit Band (MixerTrackGroupComponent& owner_) : owner (owner_) {}

    void paint (juce::Graphics& g) override
    {
        auto& track = *owner.track;
        g.fillAll (track.getViewState().getColour().withAlpha (0.35f));
        g.setColour (findColour (audium::defaultTextColourId));
        g.setFont (juce::FontOptions (11.0f, juce::Font::bold));
        g.drawText (track.getAudioTrackName(), getLocalBounds().reduced (4, 0), juce::Justification::centredLeft, true);
    }

    MixerTrackGroupComponent& owner;
};

//==============================================================================
MixerTrackGroupComponent::MixerTrackGroupComponent (MixerContext context_, std::shared_ptr<AudioTrack> track_) :
    context (std::move (context_)),
    track (std::move (track_))
{
    band = std::make_unique<Band> (*this);
    addAndMakeVisible (band.get());

    inserts = std::make_unique<InsertSlotListComponent> (context, chainAddress(), [this] (int index) { return editorTitle (index); });
    addAndMakeVisible (inserts.get());

    const auto numChannels = static_cast<int> (track->audioChannelContainer->size());
    for (int channel = 0; channel < numChannels; ++channel) {
        ios.push_back (std::make_unique<StripIo> (StripChannel { context, track, channel }));
        addAndMakeVisible (ios.back().get());
        strips.push_back (std::make_unique<MixerStripComponent> (context, track, channel));
        addAndMakeVisible (strips.back().get());
    }
}

MixerTrackGroupComponent::~MixerTrackGroupComponent() = default;

InsertChainAddress MixerTrackGroupComponent::chainAddress() const
{
    return InsertChainAddress::ofTrack (track->getId());
}

juce::String MixerTrackGroupComponent::editorTitle (int slotIndex) const
{
    juce::String effectName;
    if (auto* chain = context.chain (chainAddress()))
        if (auto slot = chain->getSlot (slotIndex))
            effectName = slot->getEffect().getType().name;
    return track->getAudioTrackName() + " / " + effectName;
}

int MixerTrackGroupComponent::getPreferredWidth() const
{
    if (strips.empty())
        return stripWidth;
    return static_cast<int> (strips.size()) * (stripWidth + stripGap) - stripGap;
}

void MixerTrackGroupComponent::setSections (int newSections)
{
    sections = newSections;
    for (auto& strip : strips)
        strip->setSections (sections);
    resized();
}

void MixerTrackGroupComponent::refresh()
{
    band->repaint();
    for (auto& io : ios)
        io->refresh();
    inserts->refresh();
    for (auto& strip : strips)
        strip->refresh();
    resized();
}

void MixerTrackGroupComponent::refreshLive()
{
    for (auto& strip : strips)
        strip->refreshLive();
}

void MixerTrackGroupComponent::resized()
{
    auto bounds = getLocalBounds();
    band->setBounds (bounds.removeFromTop (bandHeight));

    // each channel's I/O under the band, in its strip's column
    const auto showIo = (sections & Section::io) != 0;
    if (showIo) {
        bounds.removeFromTop (sectionGap);
        auto row = bounds.removeFromTop (StripIo::height);
        for (auto& io : ios) {
            io->setBounds (row.removeFromLeft (stripWidth).reduced (sidePadding, 0));
            row.removeFromLeft (stripGap);
        }
    }
    for (auto& io : ios)
        io->setVisible (showIo);

    // the track's inserts across the group
    inserts->setVisible ((sections & Section::inserts) != 0);
    if (inserts->isVisible()) {
        bounds.removeFromTop (1);
        inserts->setBounds (bounds.removeFromTop (InsertSlotListComponent::heightForRows (insertRows)).reduced (2, 0));
        bounds.removeFromTop (1);
    }

    for (auto& strip : strips) {
        strip->setBounds (bounds.removeFromLeft (stripWidth));
        bounds.removeFromLeft (stripGap);
    }
}

void MixerTrackGroupComponent::paint (juce::Graphics& g)
{
    g.fillAll (findColour (audium::secondaryBackgroundColourId));
}
