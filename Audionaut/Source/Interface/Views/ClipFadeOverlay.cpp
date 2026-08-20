//    Audionaut - Audio editing application for multitrack recordings.
//    Copyright (C) 2025 Klaus Voltmer
//
//    Audionaut uses a GPL/commercial licence - see LICENCE.md for details.

#include <JuceHeader.h>
#include "ClipFadeOverlay.h"

#include "Engine/AudiumEngine.h"
#include "Engine/Group/AudioTrack.h"
#include "Engine/Group/ResourceGroup.h"
#include "Engine/Channel/AudioChannel.h"
#include "Engine/PlayList/PlayListContainer.h"
#include "Engine/PlayList/PlayListItem.h"
#include "Engine/Region/AudioRegion.h"
#include "Engine/Resource/AudioResource.h"
#include "Engine/Resource/ChannelMapping.h"
#include "Interface/Handlers/ZoomHandler.h"
#include "Interface/Controls/DraggerControl.h"
#include "Interface/Views/WaveFormViewBase.h"
#include "Interface/Widgets/audium_AudioThumbnail.h"

using namespace juce;

ClipFadeOverlay::ClipFadeOverlay(std::shared_ptr<audium::AudioTrack> audioTrack_,
                                 std::shared_ptr<audium::AudiumEngine> audiumEngine_,
                                 std::shared_ptr<ZoomHandler> zoomHandler_) :
    audioTrack(audioTrack_),
    audiumEngine(audiumEngine_),
    zoomHandler(zoomHandler_)
{
    // transparent to the mouse itself; the lane-parented fade handles are
    // added as children and take clicks
    setInterceptsMouseClicks(false, true);
}

ClipFadeOverlay::~ClipFadeOverlay()
{
    for (auto& entry : thumbnails)
        if (entry.second != nullptr)
            entry.second->removeChangeListener(this);
}

void ClipFadeOverlay::changeListenerCallback (juce::ChangeBroadcaster*)
{
    // a ghost thumbnail finished (progressively) loading
    repaint();
}

std::shared_ptr<audium::AudioThumbnail> ClipFadeOverlay::getOrCreateThumbnail(
    const std::shared_ptr<audium::AudioResource>& resource)
{
    auto key = resource->getUrl().toString(true);

    auto it = thumbnails.find(key);
    if (it != thumbnails.end())
        return it->second;

    auto thumbnail = WaveFormViewBase::createThumbnailForResource(audiumEngine, resource);
    if (thumbnail != nullptr) {
        thumbnail->addChangeListener(this);
        // don't cache the nullptr case: a recording resource may gain a
        // reader later
        thumbnails[key] = thumbnail;
    }
    return thumbnail;
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

    // ghost waveform context: the extension pulls source material from
    // outside the region window
    auto region = item.getRegion();
    auto resourceGroup = region != nullptr ? region->getResourceGroup() : nullptr;
    auto regionSeconds = region != nullptr ? region->getRegionData(audium::seconds)
                                           : juce::Range<double>();
    auto regionLenSeconds = regionSeconds.getLength();
    auto secondsPerPx = clipW > 0.f ? regionLenSeconds / clipW : 0.0;

    // the envelope tapering the ghost, in clip-local pixel space - gainAt
    // handles x outside [0, totalWidth]. Passed unconditionally: isActive()
    // is false for a zero-length ramp fully outside the clip.
    audium::WaveformEnvelope envelope;
    envelope.totalWidth       = clipW;
    envelope.fadeInWidth      = fadeInFrac * clipW;
    envelope.fadeOutWidth     = fadeOutFrac * clipW;
    envelope.fadeInStartWidth = startFrac * clipW;
    envelope.fadeOutEndWidth  = endFrac * clipW;

    auto ghostColour = audioTrack->getViewState().getColour().withAlpha(0.4f);

    // one continuation band per channel row, so each row's FadeInOutView
    // ramp reads as an unbroken curve across the clip edge
    auto bandTop = static_cast<float>(DraggerControl::draggerHeight);

    for (auto ch = 0; ch < audioTrack->getNumAudioTrackChannels(); ++ch) {

        auto channel = audioTrack->getChannel(ch);
        if (channel == nullptr)
            break;

        auto bandHeight = static_cast<float>(channel->getChannelHeight());
        auto bandBottom = bandTop + bandHeight;

        // GHOST WAVEFORM of the extended material, dimmed, under the ramps.
        // The band's resource is the one mapped to this destination channel,
        // drawn with the same source channel / gain as the clip's row view.
        if (resourceGroup != nullptr && secondsPerPx > 0.0) {
            if (auto resource = resourceGroup->getAudioResourceAtChannel(ch)) {
                auto sourceChannel = resource->getChannelMapping().getSourceChannel();
                if (sourceChannel >= 0 &&
                    sourceChannel < resource->getNumAudioFileChannels()) {
                    if (auto thumbnail = getOrCreateThumbnail(resource)) {
                        if (thumbnail->getTotalLength() > 0.0) {

                            auto gain = static_cast<float>(dynamics.getGain(ch));

                            g.saveState();
                            // clip-local x space, so the envelope applies
                            // unchanged
                            g.addTransform(AffineTransform::translation(clipX, 0.f));
                            g.setColour(ghostColour);

                            // fade-in extension: material before the region
                            // window. The drawn source range must be clamped
                            // to the file - negative start times leave stale
                            // cache columns on the zoomed-in thumbnail path.
                            if (startFrac < 0.f) {
                                auto rawStart     = regionSeconds.getStart() + startFrac * regionLenSeconds;
                                auto clampedStart = std::max(0.0, rawStart);
                                auto endSeconds   = regionSeconds.getStart();
                                auto xLocal   = static_cast<int>(std::floor((clampedStart - endSeconds) / secondsPerPx));
                                auto widthPx  = static_cast<int>((endSeconds - clampedStart) / secondsPerPx);
                                if (widthPx > 0)
                                    thumbnail->drawChannel(g,
                                                           juce::Rectangle<int>(xLocal,
                                                                          static_cast<int>(bandTop),
                                                                          widthPx,
                                                                          static_cast<int>(bandHeight)),
                                                           clampedStart,
                                                           endSeconds,
                                                           sourceChannel,
                                                           gain,
                                                           &envelope);
                            }

                            // fade-out extension: material after the region
                            // window, clamped to the file end (missing
                            // material = silence = simply not drawn)
                            if (endFrac < 0.f) {
                                auto rawEnd       = regionSeconds.getEnd() - endFrac * regionLenSeconds;
                                auto clampedEnd   = std::min(resource->getFileLength(audium::seconds), rawEnd);
                                auto startSeconds = regionSeconds.getEnd();
                                auto widthPx  = static_cast<int>((clampedEnd - startSeconds) / secondsPerPx);
                                if (widthPx > 0)
                                    thumbnail->drawChannel(g,
                                                           juce::Rectangle<int>(static_cast<int>(clipW),
                                                                          static_cast<int>(bandTop),
                                                                          widthPx,
                                                                          static_cast<int>(bandHeight)),
                                                           startSeconds,
                                                           clampedEnd,
                                                           sourceChannel,
                                                           gain,
                                                           &envelope);
                            }

                            g.restoreState();
                        }
                    }
                }
            }
        }

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
