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
    
    audioResource->getThumbnail().addChangeListener (this);
}

AudioRegionView::~AudioRegionView()
{
    audioResource->getThumbnail().removeChangeListener(this);
}

void AudioRegionView::paint (juce::Graphics& g)
{
    
    if (audioResource != nullptr &&
        audioResource->getThumbnail().getTotalLength() > 0.0)
    {
        g.fillAll (audioResource->currentColour.withAlpha(0.25f));
        g.setColour (audioResource->currentColour);
        
        auto thumbArea = getLocalBounds();
    
#if 1 /// draw visible range
        
        // the visible range is the scrollbar's range
        auto visibleRange = zoomHandler->getVisibleRange();
        
        // adjust the drawing area
        thumbArea.setX(static_cast<int>(visibleRange.getStart()));
        thumbArea.setWidth(static_cast<int>(visibleRange.getLength()));
                       
        
        auto rangeInSeconds = zoomHandler->getVisibleRangeInSeconds();
        
        audioResource->getThumbnail().drawChannels (g, thumbArea,
                                                    rangeInSeconds.getStart(), rangeInSeconds.getEnd(), 1.0f);
        
//        std::cout << "DRAW visible start " << visibleRange.getStart() << " length " << visibleRange.getLength() << std::endl;
//        std::cout << "DRAW seconds start " << rangeInSeconds.getStart() << " length " << rangeInSeconds.getLength() << std::endl;

#else /// draw entire waveform at once
        
        auto totalRange = zoomHandler->getTotalRange();
        audioResource->getThumbnail().drawChannels (g, thumbArea,
                                                    totalRange.getStart(), totalRange.getEnd(), 1.0f);
#endif
        
        /// draw filename label
        /// offset is x = 5, y = 5
        /// background is expanded by 2 pixels
        
        g.setFont (12.0f);
        
        Rectangle<int> bonds(zoomHandler->getVisibleRange().getStart() + 5,
                             5,
                             g.getCurrentFont().getStringWidth(audioResource->getFileNameWithoutExtension()),
                             g.getCurrentFont().getHeight());
        
        g.setColour(Colours::black.withAlpha(0.25f));
        g.fillRoundedRectangle (bonds.expanded(2, 2).toFloat(), 3.0f);
        
        g.setColour (findColour(audium::defaultTextColourId));
        g.drawFittedText (audioResource->getFileNameWithoutExtension(), bonds, Justification::topLeft, 1);
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
