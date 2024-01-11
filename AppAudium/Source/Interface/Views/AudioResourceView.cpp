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


void AudioResourceView::paint (juce::Graphics& g)
{
    
    paintBackground(g);
    
    jassert(audioResource != nullptr);
    
    if (audioThumbnail->getTotalLength() > 0.0)
    {
        // the waveform colour
        g.setColour (colour);
                
        const auto start        = audioResource->getRegionData(audium::seconds).getStart();
        const auto thumbArea    = getClippedDrawingArea();
        const auto startSeconds = zoomHandler->xToSeconds(thumbArea.getX()) + start;
        const auto endSeconds   = startSeconds + zoomHandler->xToSeconds(thumbArea.getWidth());
        
        audioThumbnail->drawChannel(g, thumbArea.toNearestInt(), startSeconds, endSeconds, 0, verticalZoomFactor);
        
    }
}

void AudioResourceView::resized()
{
}
