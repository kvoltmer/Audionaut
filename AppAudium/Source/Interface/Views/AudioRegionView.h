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
    AudioRegionView(const juce::Component &parentComponent,
                    std::shared_ptr<AudiumEngine> audiumEngine,
                    std::shared_ptr<AudioResource> audioResource,
                    std::shared_ptr<ZoomHandler> zoomHandler,
                    std::shared_ptr<AudioRegion> audioRegion,
                    juce::Colour colour,
                    std::shared_ptr<RegionSelector> regionSelector,
                    int rowNumber) :
        WaveFormViewBase(parentComponent, audiumEngine, audioResource, zoomHandler, audioRegion, colour, regionSelector, rowNumber)
    {
    }

    void paint (juce::Graphics&) override;
    
private:
    
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (AudioRegionView)
};
