//    Audionaut - Audio editing application for multitrack recordings.
//    Copyright (C) 2025 Klaus Voltmer
//
//    Audionaut uses a GPL/commercial licence - see LICENCE.md for details.

#include "LoadMeter.h"
#include "LevelMeter.h"

using namespace juce;

LoadMeter::LoadMeter()
{
    setInterceptsMouseClicks (false, false);
}

LoadMeter::~LoadMeter()
{
}

Rectangle<int> LoadMeter::getBarBounds() const
{
    return getLocalBounds().reduced (spacing);
}

void LoadMeter::resized()
{
    redrawLevels();
}

void LoadMeter::redrawLevels()
{
    const auto bar = getBarBounds();
    if (bar.isEmpty())
    {
        levels = Image();
        return;
    }

    levels = Image (Image::ARGB, bar.getWidth(), bar.getHeight(), true);
    Graphics g (levels);

    const auto h = bar.getHeight();
    const auto w = bar.getWidth();

    // zone boundaries as fraction of the budget, drawn bottom-up
    auto yFor = [h] (float fraction) { return h - roundToInt (static_cast<float> (h) * fraction); };
    const int y50 = yFor (0.5f);
    const int y75 = yFor (0.75f);
    const int y90 = yFor (0.9f);

    g.setColour (MeterColours::green);
    g.fillRect (0, y50, w, h - y50);

    g.setColour (MeterColours::greenLight);
    g.fillRect (0, y75, w, y50 - y75);

    g.setColour (MeterColours::orange);
    g.fillRect (0, y90, w, y75 - y90);

    g.setColour (MeterColours::red);
    g.fillRect (0, 0, w, y90);
}

void LoadMeter::paint (Graphics& g)
{
    // frame, identical to LevelMeter
    Path indent;
    indent.addRoundedRectangle (0, 0, getWidth(), getHeight(), 1.f);
    g.setColour (Colour (0xff5e6569).withAlpha (0.4f));
    g.fillPath (indent);

    const auto bar = getBarBounds();

    if (spikeHold > 0)
    {
        // a callback overran its budget: flash the whole bar red
        g.setColour (MeterColours::red);
        g.fillRect (bar);
    }
    else if (levels.isValid())
    {
        const int h = roundToInt (static_cast<float> (bar.getHeight()) * jlimit (0.0f, 1.0f, displayLoad));
        if (h > 0)
        {
            auto area = levels.getBounds().withHeight (h).withY (bar.getHeight() - h);
            g.drawImageAt (levels.getClippedImage (area), bar.getX(), bar.getY() + area.getY());
        }

        if (peakHold > 0)
        {
            const int ph = roundToInt (static_cast<float> (bar.getHeight()) * jlimit (0.0f, 1.0f, peakLoad));
            const int peakY = jlimit (0, bar.getHeight() - 1, bar.getHeight() - ph);
            auto peakArea = levels.getBounds().withHeight (1).withY (peakY);
            g.drawImageAt (levels.getClippedImage (peakArea), bar.getX(), bar.getY() + peakY);
        }
    }

    // border
    g.setColour (Colours::black.withAlpha (0.50f));
    g.drawRoundedRectangle (getLocalBounds().toFloat(), 3.0f, 2.0f);
}

void LoadMeter::setLoad (float load, float peak)
{
    load = jmax (0.0f, load);
    peak = jmax (0.0f, peak);

    // rise instantly, fall slowly - same feel as LevelMeter's dB decrement
    if (load > displayLoad)
        displayLoad = load;
    else
        displayLoad = jmax (load, displayLoad - decrementPerFrame);

    if (peak > peakLoad)
    {
        peakLoad = peak;
        peakHold = peakHoldDuration;
    }
    else if (peakHold > 0)
    {
        --peakHold;
    }
    else
    {
        peakLoad = 0.0f;
    }

    if (peak >= 1.0f)
        spikeHold = spikeHoldDuration;
    else if (spikeHold > 0)
        --spikeHold;

    repaint();
}
