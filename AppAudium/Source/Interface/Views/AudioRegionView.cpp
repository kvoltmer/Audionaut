/*
  ==============================================================================

 AudioRegionView.cpp
    Created: 19 Sep 2023 2:20:32pm
    Author:  Klaus Voltmer

  ==============================================================================
*/

#include <JuceHeader.h>
#include "AudioRegionView.h"
#include "Interface/Handlers/ZoomHandler.h"
#include "Engine/AudioResource.h"
#include "Interface/ColourIds.h"
#include "Engine/AudioRegion.h"
#include "Engine/AudioGroup.h"
#include "Engine/PlayList/PlayListScheduler.h"

using namespace juce;

//==============================================================================
AudioRegionView::AudioRegionView(std::shared_ptr<AudioResource> resource,
                                 std::shared_ptr<ZoomHandler> zoom,
                                 std::shared_ptr<AudioRegion> region) :
    audioResource(resource),
    zoomHandler(zoom),
    audioRegion(region)
{
    // this component doesn't handle mouse events
    setInterceptsMouseClicks(false, false);
    
    // use buffered image
    setBufferedToImage(true);
        
    auto thumb = audioRegion->getAudioThumbnailForResource(audioResource);
    jassert(thumb != nullptr);
    thumb->addChangeListener(this);
}

AudioRegionView::~AudioRegionView()
{
    auto thumb = audioRegion->getAudioThumbnailForResource(audioResource);
    jassert(thumb != nullptr);
    thumb->removeChangeListener(this);
}

void AudioRegionView::paintFileNameLabel (juce::Graphics& g)
{
    /// draw filename label
    /// offset is x = 5, y = 5
    /// background is expanded by 2 pixels
    
    g.setFont (12.0f);
    
    Rectangle<int> bonds(5,
                         5,
                         g.getCurrentFont().getStringWidth(audioResource->getFileNameWithoutExtension()),
                         g.getCurrentFont().getHeight());
    
    g.setColour(Colours::black.withAlpha(0.25f));
    g.fillRoundedRectangle (bonds.expanded(2, 2).toFloat(), 3.0f);
    
    g.setColour (findColour(audium::defaultTextColourId));
    g.drawFittedText (audioResource->getFileNameWithoutExtension(), bonds, Justification::topLeft, 1);
}

void AudioRegionView::paint (juce::Graphics& g)
{
    // testing
    auto thumb = audioRegion->getAudioThumbnailForResource(audioResource);
    jassert(thumb != nullptr);
    
    jassert(audioResource != nullptr);
    
    if (thumb->getTotalLength() > 0.0)
    { 
        g.fillAll (audioRegion->getAudioGroup()->getColour().withAlpha(0.25f));
        g.setColour (audioRegion->getAudioGroup()->getColour());
        
        auto thumbArea = getLocalBounds();
    
#if 1 /// draw visible range
        

        
        
//        // the visible range is the scrollbar's range
//        auto visibleRange = zoomHandler->getVisibleRange();
//
//        // adjust the drawing area
//        thumbArea.setX(static_cast<int>(visibleRange.getStart()));
//        thumbArea.setWidth(static_cast<int>(visibleRange.getLength()));
//
//        auto rangeInSeconds = zoomHandler->getVisibleRangeInSeconds();
//
//        audioResource->getThumbnail().drawChannels (g, thumbArea,
//                                                    rangeInSeconds.getStart(), rangeInSeconds.getEnd(), 1.0f);
        
        
        auto startSecond = zoomHandler->getPlayListScheduler()->clocksToSeconds(audioRegion->position.getStart());
        auto endSecond = zoomHandler->getPlayListScheduler()->clocksToSeconds(audioRegion->position.getEnd());
        //audioResource->getThumbnail().drawChannels (g, thumbArea, startSecond, endSecond, 1.0f);
        thumb->drawChannels (g, thumbArea, startSecond, endSecond, 1.0f);
        
        
//        std::cout << "DRAW visible start " << visibleRange.getStart() << " length " << visibleRange.getLength() << std::endl;
//        std::cout << "DRAW seconds start " << rangeInSeconds.getStart() << " length " << rangeInSeconds.getLength() << std::endl;

#else /// draw entire waveform at once
        
        auto totalRange = zoomHandler->getTotalRange();
        audioResource->getThumbnail().drawChannels (g, thumbArea,
                                                    totalRange.getStart(), totalRange.getEnd(), 1.0f);
#endif
        

        paintFileNameLabel(g);
    }
    else
    {
        g.setFont (14.0f);
        g.drawFittedText ("audio resource not available", getLocalBounds(), Justification::centred, 2);
    }
    
    
    
    
//    auto thumbArea = getLocalBounds();
//    g.setColour (Colours::yellow);
//    g.fillRoundedRectangle (thumbArea.toFloat(), 3.0f);


    
}

void AudioRegionView::resized()
{
    // This method is where you should set the bounds of any child
    // components that your component contains..

}

void AudioRegionView::changeListenerCallback (ChangeBroadcaster*)
{
    // this method is called by the thumbnail when it has changed, so we should repaint it..
    repaint();
}

void AudioRegionView::setAudioResource (std::shared_ptr<AudioResource> audioResource)
{
    this->audioResource = audioResource;
}
