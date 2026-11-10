//    Audionaut - Audio editing application for multitrack recordings.
//    Copyright (C) 2025 Klaus Voltmer
//
//    Audionaut uses a GPL/commercial licence - see LICENCE.md for details.

#include "MixerBusGroupComponent.h"

#include "Interface/ColourIds.h"

using namespace audium;
using namespace MixerLayout;

MixerBusGroupComponent::MixerBusGroupComponent (MixerContext context_, bool withAuxBuses) :
    context (std::move (context_))
{
    if (withAuxBuses)
        for (const auto& bus : context.container().getAuxBuses().getBuses())
            strips.push_back (std::make_unique<MixerBusStripComponent> (context, bus->getId()));
    strips.push_back (std::make_unique<MixerBusStripComponent> (context, MixerBusStripComponent::masterBus));

    for (auto& strip : strips)
        addAndMakeVisible (strip.get());
}

MixerBusGroupComponent::~MixerBusGroupComponent() = default;

int MixerBusGroupComponent::getPreferredWidth() const
{
    return static_cast<int> (strips.size()) * (stripWidth + stripGap) - stripGap;
}

void MixerBusGroupComponent::setShown (const Shown& shown)
{
    for (auto& strip : strips)
        strip->setShown (shown);
}

void MixerBusGroupComponent::refresh()
{
    for (auto& strip : strips)
        strip->refresh();
    repaint();
}

void MixerBusGroupComponent::refreshLive()
{
    for (auto& strip : strips)
        strip->refreshLive();
}

void MixerBusGroupComponent::resized()
{
    auto bounds = getLocalBounds();
    bounds.removeFromTop (bandHeight);
    for (auto& strip : strips) {
        strip->setBounds (bounds.removeFromLeft (stripWidth));
        bounds.removeFromLeft (stripGap);
    }
}

void MixerBusGroupComponent::paint (juce::Graphics& g)
{
    g.fillAll (findColour (audium::secondaryBackgroundColourId));

    // the bands, as a track's: "Aux" in the aux colour over the aux strips, "Main" in the master's over its strip
    auto band = getLocalBounds().removeFromTop (bandHeight);
    const auto numAux = static_cast<int> (strips.size()) - 1;
    const auto drawBand = [&g, this] (juce::Rectangle<int> area, int busId, const juce::String& title)
    {
        g.setColour (MixerBusStripComponent::busColour (busId).withAlpha (0.35f));
        g.fillRect (area);
        g.setColour (findColour (audium::defaultTextColourId));
        g.setFont (juce::FontOptions (11.0f, juce::Font::bold));
        g.drawText (title, area.reduced (4, 0), juce::Justification::centredLeft, true);
    };
    if (numAux > 0) {
        drawBand (band.removeFromLeft (numAux * (stripWidth + stripGap) - stripGap), 0, TRANS ("Aux"));
        band.removeFromLeft (stripGap);
    }
    drawBand (band, MixerBusStripComponent::masterBus, TRANS ("Main"));
}
