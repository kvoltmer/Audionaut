/*
  ==============================================================================

    AudioRegionView.h
    Created: 19 Sep 2023 2:20:32pm
    Author:  Klaus Voltmer

  ==============================================================================
*/

#pragma once

#include <JuceHeader.h>
#include <memory>
#include "WaveFormViewBase.h"

class AudioResource;
class ZoomHandler;
class AudioRegion;

class AudioRegionView : public WaveFormViewBase
{
public:
    AudioRegionView(std::shared_ptr<AudiumEngine> audiumEngine,
                    std::shared_ptr<AudioResource> audioResource,
                    std::shared_ptr<ZoomHandler> zoomHandler,
                    std::shared_ptr<AudioRegion> audioRegion,
                    juce::Colour colour,
                    std::shared_ptr<RegionSelector> regionSelector) :
        WaveFormViewBase(audiumEngine, audioResource, zoomHandler, audioRegion, colour, regionSelector)
    {
    }

    void paint (juce::Graphics&) override;
    
    void updateFromEngine() override
    {
        /// TODO:
//        double posX = zoomHandler->secondsToX(audioResource->getTransportPositionSeconds());
//        double length = zoomHandler->secondsToX(audioResource->getRegionDataInSeconds().getLength());
//
//        juce::Rectangle<double> rect_tmp(posX, 0, length, audioResource->getHeight());
//        setBounds(rect_tmp.toNearestInt());
    }
//    void setRegionDataInSeconds(const juce::Range<double> newRegionData) override
//    {
//        
//    }
    
private:
    
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (AudioRegionView)
};
