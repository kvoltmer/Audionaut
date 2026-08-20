//    Audionaut - Audio editing application for multitrack recordings.
//    Copyright (C) 2025 Klaus Voltmer
//
//    Audionaut uses a GPL/commercial licence - see LICENCE.md for details.

#include <JuceHeader.h>
#include "ClipFadeOverlay.h"

#include "Engine/Group/AudioTrack.h"
#include "Engine/Channel/AudioChannel.h"
#include "Engine/PlayList/PlayListContainer.h"
#include "Engine/PlayList/PlayListItem.h"
#include "Interface/Handlers/ZoomHandler.h"
#include "Interface/Controls/DraggerControl.h"

using namespace juce;

ClipFadeOverlay::ClipFadeOverlay(std::shared_ptr<audium::AudioTrack> audioTrack_,
                                 std::shared_ptr<ZoomHandler> zoomHandler_) :
    audioTrack(audioTrack_),
    zoomHandler(zoomHandler_)
{
    // transparent to the mouse itself; the lane-parented fade handles are
    // added as children and take clicks
    setInterceptsMouseClicks(false, true);
}

void ClipFadeOverlay::paint (juce::Graphics& g)
{
    if (audioTrack == nullptr || zoomHandler == nullptr)
        return;

    auto container = audioTrack->getPlayListContainer();
    if (container == nullptr)
        return;

    const auto visibleRange = zoomHandler->getVisibleRange();

    for (const auto& item : container->getPlayListItems()) {

        if (item == nullptr)
            continue;

        auto& dynamics = item->getDynamics();
        auto startFrac = dynamics.getFadeInStart();
        auto endFrac = dynamics.getFadeOutEnd();

        // only ramp parts outside the clip are drawn here
        if (startFrac >= 0.0 && endFrac >= 0.0)
            continue;

        auto clipRange = zoomHandler->clocksToX(item->getAbsolutePositionRange(audium::clocks));

        // cull against the visible range, extended by the outside ramps
        auto extended = clipRange;
        if (startFrac < 0.0)
            extended.setStart(clipRange.getStart() + startFrac * clipRange.getLength());
        if (endFrac < 0.0)
            extended.setEnd(clipRange.getEnd() - endFrac * clipRange.getLength());

        if (! visibleRange.isEmpty() && ! extended.intersects(visibleRange))
            continue;

        paintItemExtensions(g, *item, clipRange);
    }
}

void ClipFadeOverlay::paintItemExtensions (juce::Graphics& g,
                                           const audium::PlayListItem& item,
                                           juce::Range<double> clipRange)
{
    auto& dynamics = item.getDynamics();

    auto startFrac  = static_cast<float>(dynamics.getFadeInStart());
    auto fadeInFrac = static_cast<float>(dynamics.getFadeIn());
    auto endFrac    = static_cast<float>(dynamics.getFadeOutEnd());
    auto fadeOutFrac = static_cast<float>(dynamics.getFadeOut());

    auto clipX = static_cast<float>(clipRange.getStart());
    auto clipW = static_cast<float>(clipRange.getLength());
    auto clipRight = clipX + clipW;

    // one continuation band per channel row, so each row's FadeInOutView
    // ramp reads as an unbroken curve across the clip edge
    auto bandTop = static_cast<float>(DraggerControl::draggerHeight);

    for (auto ch = 0; ch < audioTrack->getNumAudioTrackChannels(); ++ch) {

        auto channel = audioTrack->getChannel(ch);
        if (channel == nullptr)
            break;

        auto bandHeight = static_cast<float>(channel->getChannelHeight());
        auto bandBottom = bandTop + bandHeight;

        // FADE IN continuation: ramp from (rampStartX, silence) rising over
        // [rampStartX, clipX]; the inside part is drawn by the row's
        // FadeInOutView
        if (startFrac < 0.f) {
            auto rampStartX = clipX + startFrac * clipW;
            auto rampEndX   = clipX + fadeInFrac * clipW;
            auto rampWidth  = rampEndX - rampStartX;

            Path curve;
            curve.startNewSubPath(rampStartX, bandBottom);
            auto outsideWidth = static_cast<int>(clipX - rampStartX);
            auto stride = outsideWidth > 10 ? 4 : 1;
            for (auto w = 0; w < outsideWidth; w += stride) {
                auto square = static_cast<float>(audium::ClipDynamics::fadeCurve(w / rampWidth));
                curve.lineTo(rampStartX + static_cast<float>(w),
                             bandBottom - (square * bandHeight));
            }
            auto squareAtEdge = static_cast<float>(audium::ClipDynamics::fadeCurve((clipX - rampStartX) / rampWidth));
            curve.lineTo(clipX, bandBottom - (squareAtEdge * bandHeight));

            auto rounded = curve.createPathWithRoundedCorners(4);

            Path filled(rounded);
            filled.lineTo(clipX, bandBottom);
            filled.closeSubPath();

            g.setColour(Colours::white.withAlpha(0.1f));
            g.fillPath(filled);
            g.setColour(Colours::white.withAlpha(0.5f));
            g.strokePath(rounded, PathStrokeType(2.f));
        }

        // FADE OUT continuation: ramp falling to silence over
        // [clipRight, rampEndX]
        if (endFrac < 0.f) {
            auto rampStartX = clipRight - fadeOutFrac * clipW;
            auto rampEndX   = clipRight - endFrac * clipW;
            auto rampWidth  = rampEndX - rampStartX;

            Path curve;
            auto squareAtEdge = static_cast<float>(audium::ClipDynamics::fadeCurve(1.f - ((clipRight - rampStartX) / rampWidth)));
            curve.startNewSubPath(clipRight, bandBottom - (squareAtEdge * bandHeight));
            auto outsideWidth = static_cast<int>(rampEndX - clipRight);
            auto stride = outsideWidth > 10 ? 4 : 1;
            for (auto w = stride; w < outsideWidth; w += stride) {
                auto square = static_cast<float>(audium::ClipDynamics::fadeCurve(1.f - ((clipRight - rampStartX + w) / rampWidth)));
                curve.lineTo(clipRight + static_cast<float>(w),
                             bandBottom - (square * bandHeight));
            }
            curve.lineTo(rampEndX, bandBottom);

            auto rounded = curve.createPathWithRoundedCorners(4);

            Path filled(rounded);
            filled.lineTo(clipRight, bandBottom);
            filled.closeSubPath();

            g.setColour(Colours::white.withAlpha(0.1f));
            g.fillPath(filled);
            g.setColour(Colours::white.withAlpha(0.5f));
            g.strokePath(rounded, PathStrokeType(2.f));
        }

        bandTop = bandBottom;
    }
}
