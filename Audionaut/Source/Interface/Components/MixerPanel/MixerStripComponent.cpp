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
    for (auto* element : std::initializer_list<juce::Component*> { sends.get(), pan.get(), buttons.get(), fader.get(), name.get() })
        addAndMakeVisible (element);

    refresh();
}

MixerStripComponent::~MixerStripComponent() = default;

void MixerStripComponent::setSections (int newSections)
{
    if (sections == newSections)
        return;
    sections = newSections;
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
    // the elements where MixerLayout::placeStrip puts them, as tall as stripHeight counts them; a hidden section's element goes with its empty area
    const auto placement = placeStrip (getLocalBounds(), sections, static_cast<int> (channel.context.container().getAuxBuses().size()));
    const auto place = [] (juce::Component& element, juce::Rectangle<int> area)
    {
        element.setVisible (! area.isEmpty());
        element.setBounds (area);
    };
    place (*sends, placement.sendsArea);
    place (*pan, placement.panArea);
    place (*buttons, placement.buttonsArea);
    place (*fader, placement.faderArea);
    place (*name, placement.nameArea);
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
