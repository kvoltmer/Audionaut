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
        
        const auto start        = getRegionStart(audium::seconds);
        const auto thumbArea    = getClippedDrawingArea();
        const auto startSeconds = zoomHandler->xToSeconds(thumbArea.getX()) + start;
        const auto endSeconds   = startSeconds + zoomHandler->xToSeconds(thumbArea.getWidth());
        const auto channel      = audioResource->getChannelMapping().getSourceChannel();
        
        if (channel >= 0 &&
            channel < audioResource->getNumAudioFileChannels()) {
            audioThumbnail->drawChannel(g,
                                        thumbArea.toNearestInt(),
                                        startSeconds,
                                        endSeconds,
                                        channel,
                                        verticalZoomFactor * getClipGain());
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

void WaveFormViewBase::createThumbnailCache()
{
    if (audioThumbnail != nullptr) {
        audioThumbnail->removeChangeListener(this);
    }

    if (audioResource != nullptr) {
        
        // create thumbnail
        auto thumbnailCache = audiumEngine->getAudioResourceContainer()->getAudioThumbnailCache().get();
        auto formatManager = audiumEngine->getAudioResourceContainer()->getAudioFormatManager().get();
        auto sourceSamplesPerThumbnailSample = 64;
        //auto sourceSamplesPerThumbnailSample = 256;
        //auto sourceSamplesPerThumbnailSample = 4096*4;
        audioThumbnail.reset(new audium::AudioThumbnail(sourceSamplesPerThumbnailSample, *formatManager, *thumbnailCache));
        audioThumbnail->setColour(colour);
        
        if (audioResource->audioFormatReader != nullptr) {
            
            // reuse shared_ptr if the file is memory mapped
            if (dynamic_cast<MemoryMappedAudioFormatReader*> (audioResource->audioFormatReader.get()) != nullptr) {
                auto hashCode = audioResource->getUrl().toString(true).hashCode64();
                audioThumbnail->setReader(audioResource->audioFormatReader, hashCode);
            }
            else {
                // create a new input source
                if (auto inputSource = std::make_unique<juce::URLInputSource>(audioResource->getUrl())) {
                    audioThumbnail->setSource(inputSource.release());
                }
            }
            audioThumbnail->addChangeListener(this);
        }
        else {
            auto sr = audiumEngine->getAudioDeviceManager()->getCurrentAudioDevice()->getCurrentSampleRate();
            audioThumbnail->reset(1, sr);
            audioThumbnail->addChangeListener(this);
            auto busChan = channelNumber + audioResource->getAudioTrack()->getChannelOffset();
            audiumEngine->getAudioBusInterface()->setRecordingThumbnail(audioThumbnail.get(), busChan);
            
        }
        
    }
    else {
        audioThumbnail = nullptr;
    }
    
}
