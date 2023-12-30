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
#include "Engine/Group/AudioGroup.h"
#include "Engine/PlayList/PlayListScheduler.h"

using namespace juce;

//==============================================================================
void AudioRegionView::paint (juce::Graphics& g)
{
    paintBackground(g);
    
    jassert(audioRegion != nullptr);
    
    if (audioThumbnail->getTotalLength() > 0.0)
    {
        // the waveform colour
        g.setColour (colour);
        

        const auto thumbArea = getLocalBounds();
        const auto start = audioRegion->getRegionData(audium::seconds).getStart();
        const auto end = start + zoomHandler->xToSeconds(thumbArea.getWidth());
        
        audioThumbnail->drawChannel(g, thumbArea, start, end, 0, verticalZoomFactor);

    }
    else
    {
        g.setFont (14.0f);
        g.setColour(juce::Colours::white);
        g.drawFittedText ("audio data not available", getLocalBounds(), Justification::centred, 2);
    }
}
