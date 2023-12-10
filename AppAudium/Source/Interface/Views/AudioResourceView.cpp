/*
  ==============================================================================

    AudioResourceView.cpp
    Created: 27 Nov 2023 3:58:42pm
    Author:  Klaus Voltmer

  ==============================================================================
*/

#include <JuceHeader.h>
#include "AudioResourceView.h"

#include "Engine/AudiumEngine.h"
#include "Engine/AudioRegionContainer.h"

#include "Interface/Controls/RegionEditControl.h"
#include "Interface/Controls/DraggerControl.h"

void AudioResourceView::paint (juce::Graphics& g)
{
    // std::cout << "AudioResourceView::paint" << std::endl;
    
    paintBackground(g);
    
    auto thumb = audioResource->getAudioThumbnail();
    jassert(thumb != nullptr);
    
    jassert(audioResource != nullptr);
    
    if (thumb->getTotalLength() > 0.0)
    {
        // the waveform colour
        g.setColour (colour);

        // calc the absolute x offset. (our top level component is 2 levels up)
        auto absoluteOffset = getLocalPoint (getParentComponent()->getParentComponent(), juce::Point<float> {0.f, 0.f}).getX();
        
        // the visible range is the scrollbar's range.
        auto visibleRange = zoomHandler->getVisibleRange();
        
        // adjust our visible range to local range
        visibleRange = visibleRange.movedToStartAt(visibleRange.getStart() + absoluteOffset);
        
        
        auto start = audioResource->getRegionDataInSeconds().getStart();
        start += zoomHandler->xToSeconds(visibleRange.getStart());
        
        // our local bounds
        auto thumbArea = getLocalBounds();
        thumbArea = thumbArea.withX(visibleRange.getStart());
        
        if (visibleRange.getLength() > thumbArea.getWidth())
        {
            thumbArea.setWidth(visibleRange.getLength());
        }
        
        auto end = start + zoomHandler->xToSeconds(thumbArea.getWidth());

        thumb->drawChannels (g, thumbArea, start, end, verticalZoomFactor);

        
//        std::cout << this << " DRAW x = " << thumbArea.getX() << " width = " << thumbArea.getWidth();
//        std::cout << " start = " << start << " length = " << end - start;
//        std::cout << std::endl;

        //paintFileNameLabel(g);
    }
}

void AudioResourceView::updateFromEngine()
{
    double posX = zoomHandler->secondsToX(audioResource->getTransportPositionSeconds());
    double length = zoomHandler->secondsToX(audioResource->getRegionDataInSeconds().getLength());
    
    // don't change Y position
    double posY = getBounds().getY();
    juce::Rectangle<double> rect_tmp(posX, posY, length, audioResource->getHeight());
    
    setBounds(rect_tmp.toNearestInt());
    
    regionEditComponent->updateFromEngine();
}

void AudioResourceView::resized()
{
    regionEditComponent->setBounds(getLocalBounds());
}
