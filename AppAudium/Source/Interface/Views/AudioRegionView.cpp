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
void AudioRegionView::paint (juce::Graphics& g)
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

        thumb->drawChannels (g, thumbArea, start, end, verticalZoomFactor);

        
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


