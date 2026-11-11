//    Audionaut - Audio editing application for multitrack recordings.
//    Copyright (C) 2025 Klaus Voltmer
//
//    Audionaut uses a GPL/commercial licence - see LICENCE.md for details.

#include "MixerTrackGroupComponent.h"

#include "Interface/ColourIds.h"

using namespace audium;
using namespace MixerLayout;

//==============================================================================
/// The band over a track's strips with the track's name, drawn as the
/// track's header in the arrangement: the name in the track's colour on the
/// plain background, a tint of that colour when the track is selected. A
/// click selects the track as a click on that header does: plain replaces
/// the selection, Cmd adds or removes the track.
class MixerTrackGroupComponent::Band : public juce::Component
{
public:
    explicit Band (MixerTrackGroupComponent& owner_) : owner (owner_) {}

    void paint (juce::Graphics& g) override
    {
        auto& audioTrack = *owner.track;
        const auto colour = audioTrack.getViewState().getColour();
        if (audioTrack.isSelected())
            g.fillAll (colour.withAlpha (0.25f));   // the arrangement header's selection tint
        g.setColour (juce::Colours::black);
        g.drawRect (getLocalBounds(), 1);
        g.setColour (colour);
        g.setFont (juce::FontOptions (12.0f));
        g.drawText (audioTrack.getAudioTrackName(), getLocalBounds().reduced (4, 0), juce::Justification::centredLeft, true);
    }

    void mouseDown (const juce::MouseEvent& e) override
    {
        if (e.mods.isPopupMenu())
            return;

        auto& audioTrack = *owner.track;
        if (! e.mods.isAnyModifierKeyDown() && ! audioTrack.isSelected())
            audioTrack.getSelectionManager()->deselectAll();
        audioTrack.setSelected (e.mods.isCommandDown() ? ! audioTrack.isSelected() : true);

        // updateAll: the arrangement rows, the channel view and the right panel's selected-track header follow
        owner.context.container().sendActionMessage (audium::updateAll);
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
    insertsRoom.hold (*inserts);
    addAndMakeVisible (insertsRoom);

    // each channel's I/O in its strip's column, all in one room
    ioRoom.layOut = [this] (juce::Rectangle<int> full)
    {
        for (auto& io : ios) {
            io->setBounds (full.removeFromLeft (stripWidth).reduced (sidePadding, 0));
            full.removeFromLeft (stripGap);
        }
    };
    addAndMakeVisible (ioRoom);

    const auto numChannels = static_cast<int> (track->audioChannelContainer->size());
    for (int channel = 0; channel < numChannels; ++channel) {
        ios.push_back (std::make_unique<StripIo> (StripChannel { context, track, channel }));
        ios.back()->refresh();   // as the strips and the insert list do: a group rebuilt on a change of structure is not refreshed after
        ioRoom.addAndMakeVisible (ios.back().get());
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

void MixerTrackGroupComponent::setShown (const Shown& newShown)
{
    shown = newShown;
    for (auto& strip : strips)
        strip->setShown (shown);
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

    // the I/O rows under the band with their gap above, as far as their section is open
    auto ioArea = bounds.removeFromTop (ioHeight (shown));
    ioArea.removeFromTop (juce::jmin (sectionGap, ioArea.getHeight()));
    ioRoom.setRoom (ioArea, StripIo::height);

    // the track's inserts across the group, a line's gap above and below
    auto insertsArea = bounds.removeFromTop (insertsHeight (shown));
    insertsArea.removeFromTop (juce::jmin (1, insertsArea.getHeight()));
    insertsArea.removeFromBottom (juce::jmin (1, insertsArea.getHeight()));
    insertsRoom.setRoom (insertsArea.reduced (2, 0), InsertSlotListComponent::heightForRows (insertRows));

    for (auto& strip : strips) {
        strip->setBounds (bounds.removeFromLeft (stripWidth));
        bounds.removeFromLeft (stripGap);
    }
}

void MixerTrackGroupComponent::paint (juce::Graphics& g)
{
    g.fillAll (findColour (audium::secondaryBackgroundColourId));
}
