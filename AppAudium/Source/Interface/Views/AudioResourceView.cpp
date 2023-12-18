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
    
    jassert(audioResource != nullptr);
    
    if (audioThumbnail->getTotalLength() > 0.0)
    {
        // the waveform colour
        g.setColour (colour);

        const auto thumbArea = getLocalBounds();
        const auto start = audioResource->getRegionDataInSeconds().getStart();
        const auto end = start + zoomHandler->xToSeconds(thumbArea.getWidth());
        audioThumbnail->drawChannels (g, thumbArea, start, end, verticalZoomFactor);
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
