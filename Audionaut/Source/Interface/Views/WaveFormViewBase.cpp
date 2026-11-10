//    Audionaut - Audio editing application for multitrack recordings.
//    Copyright (C) 2025 Klaus Voltmer
//
//    Audionaut uses a GPL/commercial licence - see LICENCE.md for details.

#include "WaveFormViewBase.h"

#include "Interface/LookAndFeel/AudiumLookAndFeel.h"


void WaveFormViewBase::paint (juce::Graphics& g)
{
    paintBackground(g);
    
    if (audioThumbnail != nullptr &&
        audioThumbnail->getTotalLength() > 0.0) {
        // the waveform colour
        g.setColour (colour);
 
        jassert(audioResource != nullptr);
        
        const auto thumbArea    = getClippedDrawingArea();
        const auto regionStart  = getRegionStart(audium::seconds);

        // one pixel covers speedRatio-times as much source material on a
        // re-pitched clip - the zoom handler speaks timeline seconds, so
        // every timeline distance is scaled before it is added to the
        // (source-relative) region start
        const auto speedRatio   = playListItem != nullptr ? playListItem->getSpeedRatio() : 1.0;

        // Derive the drawn file time from the timeline pixel, not from the
        // region start alone. The clip component is placed at
        // floor(clocksToX(absolute position)), so local x = 0 sits up to a
        // pixel before the clip's true start; parentComponent is that clip
        // component, its x the timeline pixel of local x = 0. Rounding the
        // region start to its own pixel grid (the previous approach) used a
        // fractional part unrelated to the component's, so while the left edge
        // was dragged - region start and absolute position move together -
        // the two roundings drifted apart and the waveform jittered by a
        // pixel. Anchoring to the whole timeline pixel keeps every screen
        // column on the same file window for as long as (region start -
        // absolute position) is unchanged, which also covers loop recording.
        auto startSeconds = zoomHandler->xToSeconds(thumbArea.getX()) * speedRatio + regionStart;

        if (playListItem != nullptr && parentComponent != nullptr) {
            const auto absoluteStart = playListItem->getAbsolutePosition(audium::seconds);
            const auto clipX         = static_cast<double>(parentComponent->getBounds().getX());
            startSeconds = (zoomHandler->xToSeconds(clipX + thumbArea.getX()) - absoluteStart) * speedRatio + regionStart;
        }

        const auto endSeconds   = startSeconds + zoomHandler->xToSeconds(thumbArea.getWidth()) * speedRatio;
        const auto channel      = audioResource->getChannelMapping().getSourceChannel();
        
        // taper the waveform under the clip fades, matching the FadeInOutView
        // overlay (which spans the same local bounds)
        audium::WaveformEnvelope fadeEnvelope;
        const audium::WaveformEnvelope* envelopePtr = nullptr;

        if (playListItem != nullptr) {
            const auto& dynamics      = playListItem->getDynamics();
            fadeEnvelope.totalWidth       = static_cast<float>(getWidth());
            fadeEnvelope.fadeInWidth      = static_cast<float>(dynamics.getFadeIn()  * getWidth());
            fadeEnvelope.fadeOutWidth     = static_cast<float>(dynamics.getFadeOut() * getWidth());
            fadeEnvelope.fadeInStartWidth = static_cast<float>(dynamics.getFadeInStart() * getWidth());
            fadeEnvelope.fadeOutEndWidth  = static_cast<float>(dynamics.getFadeOutEnd()  * getWidth());
            fadeEnvelope.fadeInCurve      = static_cast<float>(dynamics.getFadeInCurve());
            fadeEnvelope.fadeOutCurve     = static_cast<float>(dynamics.getFadeOutCurve());
            if (fadeEnvelope.isActive())
                envelopePtr = &fadeEnvelope;
        }

        if (channel >= 0 &&
            channel < audioResource->getNumAudioFileChannels()) {
            audioThumbnail->drawChannel(g,
                                        thumbArea.toNearestInt(),
                                        startSeconds,
                                        endSeconds,
                                        channel,
                                        AudiumLookAndFeel::waveformScale * getClipGain(),   // 80 % of the row, as the strip's scale beside it
                                        envelopePtr,
                                        drawCache);
        }
        else {
            std::cout << "error WaveFormViewBase channel mapping." << std::endl;
        }
    }
}

void WaveFormViewBase::changeListenerCallback (juce::ChangeBroadcaster*)
{
    // this method is called by the thumbnail when it has changed, so we should repaint it..
    repaint();
}

void WaveFormViewBase::paintBackground (juce::Graphics& g)
{
    // paint a vertical gradient as background:
    
    // bottom colour = transparent version of the track colour
    const auto bottomColour = colour.withAlpha(0.375f);
    
    // top colour = complementary colour of bottom
    const auto topColour = audium::WaveFormColours::getComplementaryColour(bottomColour);
    
    auto colourGradient = juce::ColourGradient::vertical(topColour, getLocalBounds().getTopLeft().getY(),
                                                         bottomColour, getLocalBounds().getBottomLeft().getY());
    g.setGradientFill(colourGradient);
    g.fillAll();

    // a faint line above and below the centre where a full-scale sample reaches, the long
    // ticks of the channel strip's scale beside the row; only in the huge waveform size
    if (getHeight() < AudiumLookAndFeel::SizeIds::huge)
        return;
    const auto centreY = static_cast<float> (getHeight()) * 0.5f;
    const auto reach = centreY * AudiumLookAndFeel::waveformScale;
    g.setColour (colour.withAlpha (0.1f));
    for (const auto y : { centreY - reach, centreY + reach })
        g.fillRect (0.0f, y - 0.5f, static_cast<float> (getWidth()), 1.0f);
}

void WaveFormViewBase::paintFileNameLabel (juce::Graphics& g)
{
    /// draw filename label
    /// offset is x = 5, y = 5
    /// background is expanded by 2 pixels
    
    g.setFont (12.0f);
    auto label = audioResource->getFileNameWithoutExtension();
    juce::Rectangle<int> bonds(5,
                               5,
                               GlyphArrangement::getStringWidth (g.getCurrentFont(), label),
                               g.getCurrentFont().getHeight());
    
    g.setColour(juce::Colours::black.withAlpha(0.25f));
    g.fillRoundedRectangle (bonds.expanded(2, 2).toFloat(), 3.0f);
    
    g.setColour (findColour(audium::defaultTextColourId));
    g.drawFittedText (audioResource->getFileNameWithoutExtension(), bonds, juce::Justification::topLeft, 1);
}

// returns the drawing area clipped with the visible area of the viewport
// note: this is much faster than drawing the entire waveform
juce::Rectangle<double> WaveFormViewBase::getClippedDrawingArea() const
{
    // the local bounds
    auto thumbArea = getLocalBounds().toDouble();

    // the visible range of the viewport
    const auto visibleRange = zoomHandler->getVisibleRange();
    
    // clip to the area of its parent (owner)
    jassert(parentComponent);
    const auto parentOffset = static_cast<double>(parentComponent->getBounds().getX());
    const auto scrollOffset = zoomHandler->getVisibleRange().getStart();
    // whole pixels: paint() derives the drawn time range from this x and
    // hands drawChannel the rounded rect - computed from a fractional x the
    // two disagree by up to a pixel and the waveform jitters while zooming
    const auto startX = std::floor(std::max(scrollOffset - parentOffset, 0.0));
    const auto lengthX = std::min(std::ceil(visibleRange.getLength()),
                                  static_cast<double>(thumbArea.getWidth()) - startX);

    thumbArea.setX(startX);
    thumbArea.setWidth(lengthX);
    return thumbArea;
}

void WaveFormViewBase::createThumbnailCache()
{
    if (audioThumbnail != nullptr) {
        audioThumbnail->removeChangeListener(this);
    }

    if (audioResource != nullptr) {

        if (audioResource->audioFormatReader != nullptr) {
            audioThumbnail = audiumEngine->getAudioResourceContainer()->getThumbnailForResource(audioResource);
            jassert(audioThumbnail);
        }
        else {
            // get the thumbnail from the recorder
            auto busChan = channelNumber + audioResource->getAudioTrack()->getChannelOffset();
            audioThumbnail = audiumEngine->getAudioBusInterface()->getRecordingThumbnail(busChan);
            jassert(audioThumbnail);
        }
        
        // the thumbnail is shared with every other view of the file, so
        // nothing per view (the colour, say) is set on it: paint() applies
        // the view's own colour
        if (audioThumbnail != nullptr) {
            audioThumbnail->addChangeListener(this);
        }
        
    }
    else {
        audioThumbnail = nullptr;
    }
    
}
