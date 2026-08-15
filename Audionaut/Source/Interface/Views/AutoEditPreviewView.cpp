//    Audionaut - Audio editing application for multitrack recordings.
//    Copyright (C) 2025 Klaus Voltmer
//
//    Audionaut uses a GPL/commercial licence - see LICENCE.md for details.

#include <JuceHeader.h>
#include "AutoEditPreviewView.h"

using namespace juce;

void AutoEditPreviewView::paint (juce::Graphics& g)
{
    if (zoomHandler == nullptr)
        return;

    const auto fHeight = static_cast<float>(getHeight());
    const auto fWidth  = static_cast<float>(getWidth());

    // The dashes march downwards - the line is started above the top edge by
    // the animated phase, shifting the pattern.
    g.setColour (Colours::white.withAlpha (0.9f));

    const float dashLengths[] = { 6.0f, 4.0f };

    for (auto seconds : previewSegments)
    {
        const auto x = static_cast<float>(zoomHandler->secondsToX(seconds - regionStartSeconds));

        if (x < 0.0f || x > fWidth)
            continue;

        g.drawDashedLine (Line<float> (x, -dashPhase, x, fHeight),
                          dashLengths, 2, 2.0f);
    }
}

void AutoEditPreviewView::setZoomHandler(std::shared_ptr<ZoomHandler> zoomHandler_)
{
    zoomHandler = zoomHandler_;
}

void AutoEditPreviewView::setPreviewSegments(std::vector<float> previewSegments_,
                                             double regionStartSeconds_)
{
    previewSegments = std::move(previewSegments_);
    regionStartSeconds = regionStartSeconds_;

    if (previewSegments.empty())
        stopTimer();
    else if (! isTimerRunning())
        startTimerHz (25);

    repaint();
}

void AutoEditPreviewView::timerCallback()
{
    // One dash period is the sum of the dash lengths (see paint), so the
    // phase wraps seamlessly.
    dashPhase = std::fmod (dashPhase + 1.0f, 10.0f);
    repaint();
}
