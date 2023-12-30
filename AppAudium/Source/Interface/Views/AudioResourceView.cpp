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
    // std::cout << "AudioResourceView::paint" << std::endl;
    
    paintBackground(g);
    
    jassert(audioResource != nullptr);
    
    if (audioThumbnail->getTotalLength() > 0.0)
    {
        // the waveform colour
        g.setColour (colour);

        const auto thumbArea = getLocalBounds();
        const auto start = audioResource->getRegionData(audium::seconds).getStart();
        const auto end = start + zoomHandler->xToSeconds(thumbArea.getWidth());
        
        //rowNumber
        audioThumbnail->drawChannel(g, thumbArea, start, end, 0, verticalZoomFactor);
    }
}

void AudioResourceView::resized()
{
}
