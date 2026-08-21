//    Audionaut - Audio editing application for multitrack recordings.
//    Copyright (C) 2025 Klaus Voltmer
//
//    Audionaut uses a GPL/commercial licence - see LICENCE.md for details.

#include <JuceHeader.h>
#include "FadeInOutView.h"

using namespace juce;

void FadeInOutView::paint (juce::Graphics& g)
{
    if (playListItem == nullptr)
        return;
    
    
    auto& dynamics      = playListItem->getDynamics();
    auto fHeight        = static_cast<float>(getHeight());
    auto fWidth         = static_cast<float>(getWidth());
    auto yOffset        = 1.f;

    // the fade-in ramp runs from fadeInStartX up to fadeInEndX, the fade-out
    // ramp from fadeOutStartX down to fadeOutEndX; audio outside
    // [fadeInStartX, fadeOutEndX] is silent. negative fractions put the ramp
    // origin outside the clip (the fade extends the audible material); the
    // off-clip part is clipped away here and drawn by the lane's
    // ClipFadeOverlay instead
    auto fFadeInStartX  = static_cast<float>(dynamics.getFadeInStart()) * fWidth;
    auto fFadeInEndX    = static_cast<float>(dynamics.getFadeIn()) * fWidth;
    auto fFadeOutStartX = fWidth - static_cast<float>(dynamics.getFadeOut()) * fWidth;
    auto fFadeOutEndX   = fWidth - static_cast<float>(dynamics.getFadeOutEnd()) * fWidth;

    // fadeInStart <= fadeIn and fadeOutEnd <= fadeOut, so a non-zero offset
    // implies a non-zero fade
    auto hasFadeIn      = static_cast<int>(fFadeInEndX) > 0;
    auto hasFadeOut     = static_cast<int>(fWidth - fFadeOutStartX) > 0;

    // dim the silent head/tail
    g.setColour(Colours::black.withAlpha(0.35f));
    if (fFadeInStartX >= 1.f)
        g.fillRect(0.f, 0.f, fFadeInStartX, fHeight);
    if (fWidth - fFadeOutEndX >= 1.f)
        g.fillRect(fFadeOutEndX, 0.f, fWidth - fFadeOutEndX, fHeight);

    // the bend-handle node is an open ring threaded onto the line: the
    // stroke breaks tightly around it. only on the first channel row, while
    // the clip is selected, on ramps wide enough to grab, and only when the
    // midpoint lies inside this view (else the lane's ClipFadeOverlay draws
    // it)
    constexpr auto nodeRadius = 3.f;
    constexpr auto gapRadius = 4.f;
    constexpr auto minNodeRampWidth = 16.f;

    auto nodesActive = showCurveNodes && playListItem->isSelected();

    auto fadeInMidX = (fFadeInStartX + fFadeInEndX) / 2.f;
    auto fadeInNode = nodesActive && hasFadeIn
                   && (fFadeInEndX - fFadeInStartX) > minNodeRampWidth
                   && fadeInMidX >= 0.f && fadeInMidX <= fWidth;

    auto fadeOutMidX = (fFadeOutStartX + fFadeOutEndX) / 2.f;
    auto fadeOutNode = nodesActive && hasFadeOut
                    && (fFadeOutEndX - fFadeOutStartX) > minNodeRampWidth
                    && fadeOutMidX >= 0.f && fadeOutMidX <= fWidth;

    // the fill stays continuous; only the stroke is interrupted at a node
    juce::Path fillPath;
    juce::Path strokePath;
    auto strokeOpen = false;

    auto addPoint = [&](float x, float y, float gapMidX, bool hasGap) {
        fillPath.lineTo(x, y);

        if (hasGap && std::abs(x - gapMidX) <= gapRadius) {
            strokeOpen = false;
            return;
        }
        if (! strokeOpen) {
            strokePath.startNewSubPath(x, y);
            strokeOpen = true;
        }
        else {
            strokePath.lineTo(x, y);
        }
    };

    auto startAt = [&](float x, float y) {
        fillPath.startNewSubPath(x, y);
        strokePath.startNewSubPath(x, y);
        strokeOpen = true;
    };

    if (hasFadeIn) {
        auto fRampWidth = fFadeInEndX - fFadeInStartX;
        auto iRampWidth = static_cast<int>(fRampWidth);

        startAt(fFadeInStartX, fHeight);
        auto stride = iRampWidth > 10 ? 4 : 1;
        for (auto w = 0; w < iRampWidth; w += stride) {
            auto square = static_cast<float>(audium::ClipDynamics::fadeCurve(w / fRampWidth, dynamics.getFadeInCurve()));
            auto y      = fHeight - (square * fHeight);

            addPoint(fFadeInStartX + static_cast<float>(w), y, fadeInMidX, fadeInNode);
        }
        addPoint(fFadeInEndX, yOffset, fadeInMidX, fadeInNode);
        if (! hasFadeOut) {
            addPoint(fWidth, yOffset, 0.f, false);
            addPoint(fWidth, fHeight, 0.f, false);
        }
    }

    if (hasFadeOut) {

        if (! hasFadeIn) {
            startAt(0.f, fHeight);
            addPoint(0.f, yOffset, 0.f, false);
        }

        auto fRampWidth = fFadeOutEndX - fFadeOutStartX;
        auto iRampWidth = static_cast<int>(fRampWidth);

        addPoint(fFadeOutStartX, yOffset, fadeOutMidX, fadeOutNode);
        auto stride = iRampWidth > 10 ? 4 : 1;
        for (auto w = 0; w < iRampWidth; w += stride) {
            auto square = static_cast<float>(audium::ClipDynamics::fadeCurve(1.f - (w / fRampWidth), dynamics.getFadeOutCurve()));
            auto y      = fHeight - (square * fHeight);

            addPoint(fFadeOutStartX + static_cast<float>(w), y, fadeOutMidX, fadeOutNode);
        }
        addPoint(fFadeOutEndX, fHeight, fadeOutMidX, fadeOutNode);
    }


    if (hasFadeIn ||
        hasFadeOut) {

        g.setColour(Colour(Colours::white).withAlpha(0.1f));
        g.fillPath(fillPath.createPathWithRoundedCorners(4));

        g.setColour(Colour(Colours::white).withAlpha(0.5f));
        g.strokePath(strokePath.createPathWithRoundedCorners(4), PathStrokeType (2.f));

        // the node rings, threaded onto the line
        if (fadeInNode) {
            auto midY = fHeight - static_cast<float>(audium::ClipDynamics::fadeCurve(0.5, dynamics.getFadeInCurve())) * fHeight;
            g.drawEllipse(fadeInMidX - nodeRadius, midY - nodeRadius,
                          nodeRadius * 2.f, nodeRadius * 2.f, 1.5f);
        }
        if (fadeOutNode) {
            auto midY = fHeight - static_cast<float>(audium::ClipDynamics::fadeCurve(0.5, dynamics.getFadeOutCurve())) * fHeight;
            g.drawEllipse(fadeOutMidX - nodeRadius, midY - nodeRadius,
                          nodeRadius * 2.f, nodeRadius * 2.f, 1.5f);
        }
    }

}

void FadeInOutView::resized()
{
}

void FadeInOutView::setPlayListItem(std::shared_ptr<audium::PlayListItem> item)
{
    playListItem = item;
}
