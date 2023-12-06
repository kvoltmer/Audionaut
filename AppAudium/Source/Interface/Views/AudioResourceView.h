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
class RegionSelector;

class AudioResourceView  : public AudioViewBase
{
public:
    AudioResourceView(std::shared_ptr<AudioResource> audioResource,
                      std::shared_ptr<ZoomHandler> zoomHandler,
                      std::shared_ptr<AudioRegion> audioRegion,
                      juce::Colour colour,
                      std::shared_ptr<RegionSelector> regionSelector) :
        AudioViewBase(audioResource, zoomHandler, audioRegion, colour, regionSelector)
    {
    }

    void paint (juce::Graphics&) override;


    
    void updateFromEngine() override
    {
        double posX = zoomHandler->secondsToX(audioResource->getTransportPosition());
        double length = zoomHandler->secondsToX(audioResource->getRegionDataInSeconds().getLength());
        
        // don't change Y position
        double posY = getBounds().getY();
        juce::Rectangle<double> rect_tmp(posX, posY, length, audioResource->getHeight());
        
        setBounds(rect_tmp.toNearestInt());
    }
    
private:
    
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (AudioResourceView)
};
