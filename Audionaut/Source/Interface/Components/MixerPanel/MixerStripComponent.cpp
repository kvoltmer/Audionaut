//    Audionaut - Audio editing application for multitrack recordings.
//    Copyright (C) 2025 Klaus Voltmer
//
//    Audionaut uses a GPL/commercial licence - see LICENCE.md for details.

#include "MixerStripComponent.h"

#include "Interface/ColourIds.h"

using namespace audium;
using namespace MixerLayout;

MixerStripComponent::MixerStripComponent (MixerContext context, std::shared_ptr<AudioTrack> track, int channelNumber) :
    channel { std::move (context), std::move (track), channelNumber }
{
    sends = std::make_unique<StripSends> (channel);
    pan = std::make_unique<StripPan> (channel);
    buttons = std::make_unique<StripButtons> (channel);
    fader = std::make_unique<StripFader> (channel);
    name = std::make_unique<StripName> (channel);
    sendsRoom.hold (*sends);
    panRoom.hold (*pan);
    buttonsRoom.hold (*buttons);
    for (auto* element : std::initializer_list<juce::Component*> { &sendsRoom, &panRoom, &buttonsRoom, fader.get(), name.get() })
        addAndMakeVisible (element);

    refresh();
}

MixerStripComponent::~MixerStripComponent() = default;

void MixerStripComponent::setShown (const Shown& newShown)
{
    if (shown == newShown)
        return;
    shown = newShown;
    resized();
}

void MixerStripComponent::refresh()
{
    if (channel.channel() == nullptr)
        return;

    sends->refresh();
    pan->refresh();
    buttons->refresh();
    fader->refresh();
    name->refresh();
    repaint();   // the selection tint
}

void MixerStripComponent::refreshLive()
{
    fader->refreshLive();
}

void MixerStripComponent::resized()
{
    // the elements where MixerLayout::placeStrip puts them, as tall as stripHeight counts them; a section's room
    // has as much of its element's height as the section is open (none while hidden)
    const auto numAuxBuses = static_cast<int> (channel.context.container().getAuxBuses().size());
    const auto placement = placeStrip (getLocalBounds(), shown, numAuxBuses);
    sendsRoom.setRoom (placement.sendsArea, StripSends::heightFor (numAuxBuses));
    panRoom.setRoom (placement.panArea, StripPan::height);
    buttonsRoom.setRoom (placement.buttonsArea, StripButtons::height);
    fader->setBounds (placement.faderArea);
    name->setBounds (placement.nameArea);
}

void MixerStripComponent::paint (juce::Graphics& g)
{
    auto ch = channel.channel();
    const auto selected = ch != nullptr && ch->isSelected();

    g.fillAll (findColour (audium::backgroundColourId).darker (0.35f));
    if (selected) {
        g.setColour (findColour (audium::defaultHighlightColourId).withAlpha (0.12f));
        g.fillAll();
    }
}
