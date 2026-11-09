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
    // the elements from the top, each as tall as MixerLayout::stripHeight counts it; the name bar at the bottom, the fader above it
    auto bounds = getLocalBounds();
    name->setBounds (bounds.removeFromBottom (StripName::height));
    bounds.reduce (sidePadding, 0);
    bounds.removeFromTop (sectionGap);

    const auto place = [&bounds] (juce::Component& element, bool shown, int height, int gapAfter)
    {
        element.setVisible (shown);
        if (! shown)
            return;
        element.setBounds (bounds.removeFromTop (height));
        bounds.removeFromTop (gapAfter);
    };
    place (*sends, shows (Section::sends), StripSends::heightFor (channel.context.container().getAuxBuses().size()), sectionGap);
    place (*pan, shows (Section::pan), StripPan::height, 0);
    place (*buttons, shows (Section::buttons), StripButtons::height, 0);
    fader->setBounds (bounds.removeFromTop (StripFader::height));
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
