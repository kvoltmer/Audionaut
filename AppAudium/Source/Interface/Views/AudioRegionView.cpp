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
    // setBufferedToImage(true);
        
    auto thumb = audioResource->getAudioThumbnail();
    jassert(thumb != nullptr);
    thumb->addChangeListener(this);
}

AudioRegionView::~AudioRegionView()
{
    auto thumb = audioResource->getAudioThumbnail();
    jassert(thumb != nullptr);
    thumb->removeChangeListener(this);
}

void AudioRegionView::paintBackground (juce::Graphics& g)
{
    // paint a vertical gradient as background:
    
    // bottom colour = transparent version of the group colour
    const auto bottomColour = audioRegion->getAudioGroup()->getColour().withAlpha(0.375f);
    
    // top colour = complementary colour of bottom
    const auto topColour = audium::getComplementaryColour(bottomColour);
    
    auto colourGradient = juce::ColourGradient::vertical(topColour, getLocalBounds().getTopLeft().getY(),
                                                         bottomColour, getLocalBounds().getBottomLeft().getY());
    g.setGradientFill(colourGradient);
    g.fillAll();
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
    paintBackground(g);
    
    auto thumb = audioResource->getAudioThumbnail();
    jassert(thumb != nullptr);
    
    jassert(audioResource != nullptr);
    
    if (thumb->getTotalLength() > 0.0)
    {
        // the waveform colour
        g.setColour (audioRegion->getAudioGroup()->getColour());
        
        // calc the absolute x offset. (our top level component is 2 levels up)
        auto absoluteOffset = getLocalPoint (getParentComponent()->getParentComponent(), juce::Point<float> {0.f, 0.f}).getX();
        
        // the visible range is the scrollbar's range.
        auto visibleRange = zoomHandler->getVisibleRange();
        
        // adjust our visible range to local range
        visibleRange = visibleRange.movedToStartAt(visibleRange.getStart() + absoluteOffset);
        
        
        auto start = audioRegion->getRegionDataInSeconds().getStart();
        start += zoomHandler->xToSeconds(visibleRange.getStart());
        
        // our local bounds
        auto thumbArea = getLocalBounds();
        thumbArea = thumbArea.withX(visibleRange.getStart());
        
        if (visibleRange.getLength() > thumbArea.getWidth())
        {
            thumbArea.setWidth(visibleRange.getLength());
        }
        
        auto end = start + zoomHandler->xToSeconds(thumbArea.getWidth());

        thumb->drawChannels (g, thumbArea, start, end, 1.0f);

        
//        std::cout << this << " DRAW x = " << thumbArea.getX() << " width = " << thumbArea.getWidth();
//        std::cout << " start = " << start << " length = " << end - start;
//        std::cout << std::endl;
        

        paintFileNameLabel(g);
    }
    else
    {
        g.setFont (14.0f);
        g.setColour(juce::Colours::white);
        g.drawFittedText ("audio data not available", getLocalBounds(), Justification::centred, 2);
    }


    
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
