/*
  ==============================================================================

    AudioResourceView.h
    Created: 27 Nov 2023 3:58:42pm
    Author:  Klaus Voltmer

  ==============================================================================
*/

#pragma once

#include <JuceHeader.h>
#include "AudioViewBase.h"

class AudioResource;
class ZoomHandler;
class AudioRegion;

class AudioResourceView  : public AudioViewBase
{
public:
    AudioResourceView(std::shared_ptr<AudioResource> audioResource,
                      std::shared_ptr<ZoomHandler> zoomHandler,
                      std::shared_ptr<AudioRegion> audioRegion,
                      juce::Colour colour) :
        AudioViewBase(audioResource, zoomHandler, audioRegion, colour)
    {
    }

    void paint (juce::Graphics&) override;

private:
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (AudioResourceView)
};
