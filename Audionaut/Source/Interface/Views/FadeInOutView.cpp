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

    // the curves are editing UI: unselected clips show their fades through
    // the tapered waveform alone
    if (! playListItem->isSelected())
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

    // only the two ramp curves are drawn - no plateau or clip-edge lines.
    // the area OUTSIDE each curve (the attenuated part, above the ramp) is
    // darkened a little. the bend handle draws its own circle on top.
    juce::Path fillPath;
    juce::Path strokePath;

    if (hasFadeIn) {
        auto fRampWidth = fFadeInEndX - fFadeInStartX;
        auto iRampWidth = static_cast<int>(fRampWidth);

        juce::Path curve;
        curve.startNewSubPath (Point<float> (fFadeInStartX, fHeight));
        auto stride = iRampWidth > 10 ? 4 : 1;
        for (auto w = 0; w < iRampWidth; w += stride) {
            auto square = static_cast<float>(audium::ClipDynamics::fadeCurve(w / fRampWidth, dynamics.getFadeInCurve()));
            auto y      = fHeight - (square * fHeight);

            curve.lineTo(fFadeInStartX + static_cast<float>(w), y);
        }
        curve.lineTo(fFadeInEndX, yOffset);

        strokePath.addPath(curve);

        // the wedge ABOVE the curve (fill only)
        curve.lineTo(fFadeInStartX, 0.f);
        curve.closeSubPath();
        fillPath.addPath(curve);
    }

    if (hasFadeOut) {
        auto fRampWidth = fFadeOutEndX - fFadeOutStartX;
        auto iRampWidth = static_cast<int>(fRampWidth);

        juce::Path curve;
        curve.startNewSubPath (Point<float> (fFadeOutStartX, yOffset));
        auto stride = iRampWidth > 10 ? 4 : 1;
        for (auto w = 0; w < iRampWidth; w += stride) {
            auto square = static_cast<float>(audium::ClipDynamics::fadeCurve(1.f - (w / fRampWidth), dynamics.getFadeOutCurve()));
            auto y      = fHeight - (square * fHeight);

            curve.lineTo(fFadeOutStartX + static_cast<float>(w), y);
        }
        curve.lineTo(fFadeOutEndX, fHeight);

        strokePath.addPath(curve);

        // the wedge ABOVE the curve (fill only)
        curve.lineTo(fFadeOutEndX, 0.f);
        curve.closeSubPath();
        fillPath.addPath(curve);
    }


    if (hasFadeIn ||
        hasFadeOut) {

        g.setColour(Colours::black.withAlpha(0.18f));
        g.fillPath(fillPath.createPathWithRoundedCorners(4));

        g.setColour(Colour(Colours::white).withAlpha(0.5f));
        g.strokePath(strokePath.createPathWithRoundedCorners(4), PathStrokeType (2.f));
    }

}

void FadeInOutView::resized()
{
}

void FadeInOutView::setPlayListItem(std::shared_ptr<audium::PlayListItem> item)
{
    playListItem = item;
}
