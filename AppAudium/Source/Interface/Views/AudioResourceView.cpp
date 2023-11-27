/*
  ==============================================================================

    AudioResourceView.cpp
    Created: 27 Nov 2023 3:58:42pm
    Author:  Klaus Voltmer

  ==============================================================================
*/

#include <JuceHeader.h>
#include "AudioResourceView.h"

//==============================================================================
void AudioResourceView::paint (juce::Graphics& g)
{
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
        
        
        auto start = 0.0;//audioRegion->getRegionDataInSeconds().getStart();
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
}

