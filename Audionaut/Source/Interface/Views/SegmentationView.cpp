//    Audionaut - Audio editing application for multitrack recordings.
//    Copyright (C) 2025 Klaus Voltmer
//
//    Audionaut uses a GPL/commercial licence - see LICENCE.md for details.

#include <JuceHeader.h>
#include "SegmentationView.h"

using namespace juce;

juce::Colour SegmentationView::colourForType(audium::AnalysisType analysisType)
{
    switch (analysisType)
    {
        case audium::AnalysisType::SBic:       return Colours::orange;
        case audium::AnalysisType::Onset:      return Colours::cyan;
        case audium::AnalysisType::Beat:       return Colours::yellow;
        case audium::AnalysisType::BeatDegara: return Colours::magenta;
    }

    return Colours::white;
}

void SegmentationView::paint (juce::Graphics& g)
{
    if (zoomHandler == nullptr)
        return;

    const auto fHeight = static_cast<float>(getHeight());
    const auto fWidth  = static_cast<float>(getWidth());

    for (const auto& [analysisType, segments] : segmentsByType)
    {
        // Onsets are drawn as short top ticks; SBic/Beat boundaries span the
        // full height so they read as region divisions.
        const auto isOnset = (analysisType == audium::AnalysisType::Onset);
        const auto lineBottom = isOnset ? fHeight * 0.25f : fHeight;

        g.setColour (colourForType(analysisType).withAlpha (0.7f));

        for (auto seconds : segments)
        {
            const auto x = static_cast<float>(zoomHandler->secondsToX(seconds - regionStartSeconds));

            if (x < 0.0f || x > fWidth)
                continue;

            g.drawLine (x, 0.0f, x, lineBottom, 1.0f);
        }
    }
}

void SegmentationView::resized()
{
}

void SegmentationView::setZoomHandler(std::shared_ptr<ZoomHandler> zoomHandler_)
{
    zoomHandler = zoomHandler_;
}

void SegmentationView::setSegments(std::unordered_map<audium::AnalysisType, std::vector<float>> segmentsByType_,
                                   double regionStartSeconds_)
{
    segmentsByType = std::move(segmentsByType_);
    regionStartSeconds = regionStartSeconds_;
    repaint();
}
