//    Audionaut - Audio editing application for multitrack recordings.
//    Copyright (C) 2025 Klaus Voltmer
//
//    Audionaut uses a GPL/commercial licence - see LICENCE.md for details.

#pragma once

#include <JuceHeader.h>
#include <memory>
#include <vector>

#include "Interface/Handlers/ZoomHandler.h"

/**
 * @class AutoEditPreviewView
 * @brief Overlay above AudioClipView that draws the merge preview of a pending
 *        auto edit (AnalysisProvider::getMergePreview) as animated dashed
 *        lines - the cuts the edit would make.
 *
 * Follows the same pattern as SegmentationView: a lightweight juce::Component
 * child of AudioClipView that renders in paint() from data supplied by its
 * owner. Preview timestamps are file-relative seconds; they are mapped to
 * region-local x with the ZoomHandler, offset by the region's source start.
 *
 * The dash animation only runs while a preview is shown.
 */
class AutoEditPreviewView : public juce::Component,
                            private juce::Timer
{
public:
    AutoEditPreviewView() = default;
    ~AutoEditPreviewView() override = default;

    void paint (juce::Graphics&) override;

    void setZoomHandler(std::shared_ptr<ZoomHandler> zoomHandler_);

    /**
     * @brief Sets the preview boundaries to display. Empty hides the preview
     *        and stops the animation.
     * @param previewSegments    Boundary timestamps (seconds).
     * @param regionStartSeconds The region's source offset into the file (seconds).
     */
    void setPreviewSegments(std::vector<float> previewSegments,
                            double regionStartSeconds);

private:
    // Advances the dash phase; only running while a preview is shown.
    void timerCallback() override;

    std::shared_ptr<ZoomHandler> zoomHandler;
    std::vector<float> previewSegments;
    double regionStartSeconds = 0.0;
    float dashPhase = 0.0f;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (AutoEditPreviewView)
};
