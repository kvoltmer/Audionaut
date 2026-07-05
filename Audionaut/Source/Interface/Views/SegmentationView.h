//    Audionaut - Audio editing application for multitrack recordings.
//    Copyright (C) 2025 Klaus Voltmer
//
//    Audionaut uses a GPL/commercial licence - see LICENCE.md for details.

#pragma once

#include <JuceHeader.h>
#include <memory>
#include <unordered_map>
#include <vector>

#include "Interface/Handlers/ZoomHandler.h"
#include "Engine/Analysis/AnalysisCache.h"

/**
 * @class SegmentationView
 * @brief Overlay that draws the stored analysis results (SBic segment
 *        boundaries, onsets, ...) as vertical markers over a region.
 *
 * Follows the same pattern as FadeInOutView: a lightweight juce::Component
 * child of AudioRegionView that renders in paint() from data supplied by its
 * owner. Segment timestamps are file-relative seconds; they are mapped to
 * region-local x with the ZoomHandler, offset by the region's source start.
 */
class SegmentationView : public juce::Component
{
public:
    SegmentationView() = default;
    ~SegmentationView() override = default;

    void paint (juce::Graphics&) override;
    void resized() override;

    void setZoomHandler(std::shared_ptr<ZoomHandler> zoomHandler_);

    /**
     * @brief Sets the results to display.
     * @param segmentsByType     Boundary timestamps (seconds) per analysis type.
     * @param regionStartSeconds The region's source offset into the file (seconds).
     */
    void setSegments(std::unordered_map<audium::AnalysisType, std::vector<float>> segmentsByType,
                     double regionStartSeconds);

private:
    static juce::Colour colourForType(audium::AnalysisType analysisType);

    std::shared_ptr<ZoomHandler> zoomHandler;
    std::unordered_map<audium::AnalysisType, std::vector<float>> segmentsByType;
    double regionStartSeconds = 0.0;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (SegmentationView)
};
