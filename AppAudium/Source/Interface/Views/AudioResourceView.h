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
class RegionEditControl;
class AudiumEngine;

class AudioResourceView  : public AudioViewBase
{
public:
    AudioResourceView(std::shared_ptr<AudiumEngine> audiumEngine,
                      std::shared_ptr<AudioResource> audioResource,
                      std::shared_ptr<ZoomHandler> zoomHandler,
                      std::shared_ptr<AudioRegion> audioRegion,
                      juce::Colour colour,
                      std::shared_ptr<RegionSelector> regionSelector) :
        AudioViewBase(audioResource, zoomHandler, audioRegion, colour, regionSelector),
        audiumEngine(audiumEngine)
    {
        rebuildComponents();
    }

    void paint (juce::Graphics&) override;

    void resized() override;
    
    void updateFromEngine() override;
    bool mustRebuildComponents() const;
    void rebuildComponents();
    
private:
    
    std::vector<std::shared_ptr<RegionEditControl>> regionEditControls;
    
    std::shared_ptr<AudiumEngine> audiumEngine;
    
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (AudioResourceView)
};
