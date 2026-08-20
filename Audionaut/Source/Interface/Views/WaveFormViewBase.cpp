//    Audionaut - Audio editing application for multitrack recordings.
//    Copyright (C) 2025 Klaus Voltmer
//
//    Audionaut uses a GPL/commercial licence - see LICENCE.md for details.

#include "WaveFormViewBase.h"

void WaveFormViewBase::paint (juce::Graphics& g)
{
    paintBackground(g);
    
    if (audioThumbnail != nullptr &&
        audioThumbnail->getTotalLength() > 0.0) {
        // the waveform colour
        g.setColour (colour);
 
        jassert(audioResource != nullptr);
        
        auto start              = getRegionStart(audium::seconds);
        // convert region start to integer x values (avoids jitter when recording in a loop)
        start                   = zoomHandler->reinterpretSeconds(start);
        const auto thumbArea    = getClippedDrawingArea();
        const auto startSeconds = zoomHandler->xToSeconds(thumbArea.getX()) + start;
        const auto endSeconds   = startSeconds + zoomHandler->xToSeconds(thumbArea.getWidth());
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
                                        verticalZoomFactor * getClipGain(),
                                        envelopePtr);
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
    //std::cout << parentComponent.getName().toStdString() << " offset: " << parentOffset << " " << scrollOffset << std::endl;
    const auto startX = std::max(scrollOffset - parentOffset, 0.0);
    const auto lengthX = std::min(visibleRange.getLength(), static_cast<double>(thumbArea.getWidth()) - startX);

    thumbArea.setX(startX);
    thumbArea.setWidth(lengthX);
    //std::cout << thumbArea.getX() << " " << thumbArea.getWidth() << std::endl;
    return thumbArea;
}

std::shared_ptr<audium::AudioThumbnail> WaveFormViewBase::createThumbnailForResource(
    const std::shared_ptr<audium::AudiumEngine>& audiumEngine,
    const std::shared_ptr<audium::AudioResource>& audioResource)
{
    if (audioResource == nullptr ||
        audioResource->audioFormatReader == nullptr)
        return nullptr;

    auto thumbnailCache = audiumEngine->getAudioResourceContainer()->getAudioThumbnailCache().get();
    auto formatManager = audiumEngine->getAudioResourceContainer()->getAudioFormatManager().get();
    auto sourceSamplesPerThumbnailSample = 64;
    auto thumbnail = std::make_shared<audium::AudioThumbnail>(sourceSamplesPerThumbnailSample,
                                                              *formatManager,
                                                              *thumbnailCache);
    // reuse shared_ptr if the file is memory mapped
    if (dynamic_cast<MemoryMappedAudioFormatReader*> (audioResource->audioFormatReader.get()) != nullptr) {
        auto hashCode = audioResource->getUrl().toString(true).hashCode64();
        thumbnail->setReader(audioResource->audioFormatReader, hashCode);
    }
    else {
        // create a new input source
        if (auto inputSource = std::make_unique<juce::URLInputSource>(audioResource->getUrl())) {
            thumbnail->setSource(inputSource.release());
        }
    }
    return thumbnail;
}

void WaveFormViewBase::createThumbnailCache()
{
    if (audioThumbnail != nullptr) {
        audioThumbnail->removeChangeListener(this);
    }

    if (audioResource != nullptr) {

        if (audioResource->audioFormatReader != nullptr) {
            audioThumbnail = createThumbnailForResource(audiumEngine, audioResource);
            jassert(audioThumbnail);
        }
        else {
            // get the thumbnail from the recorder
            auto busChan = channelNumber + audioResource->getAudioTrack()->getChannelOffset();
            audioThumbnail = audiumEngine->getAudioBusInterface()->getRecordingThumbnail(busChan);
            jassert(audioThumbnail);
        }
        
        if (audioThumbnail != nullptr) {
            audioThumbnail->addChangeListener(this);
            audioThumbnail->setColour(colour);
        }
        
    }
    else {
        audioThumbnail = nullptr;
    }
    
}
