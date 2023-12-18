/*
  ==============================================================================

    AudioResourceView.h
    Created: 27 Nov 2023 3:58:42pm
    Author:  Klaus Voltmer

  ==============================================================================
*/

#pragma once

#include <JuceHeader.h>
#include "Interface/Views/WaveFormViewBase.h"
#include "Interface/Components/MiddlePanel/EditView/RegionEditComponent.h"

class AudioResource;
class ZoomHandler;
class AudioRegion;
class RegionSelector;
class RegionEditControl;
class AudiumEngine;


class AudioResourceView  : public WaveFormViewBase
{
public:
    AudioResourceView(std::shared_ptr<AudiumEngine> audiumEngine,
                      std::shared_ptr<AudioResource> audioResource,
                      std::shared_ptr<ZoomHandler> zoomHandler,
                      std::shared_ptr<AudioRegion> audioRegion,
                      juce::Colour colour,
                      std::shared_ptr<RegionSelector> regionSelector) :
        WaveFormViewBase(audiumEngine, audioResource, zoomHandler, audioRegion, colour, regionSelector)
    {
        regionEditComponent = std::shared_ptr<RegionEditComponent> (new RegionEditComponent(audiumEngine,
                                                                                            audioResource,
                                                                                            zoomHandler,
                                                                                            regionSelector));
        addAndMakeVisible(regionEditComponent.get());
    }

    void paint (juce::Graphics&) override;

    void resized() override;
    
    void updateFromEngine() override;
    
private:
    
    std::vector<std::shared_ptr<RegionEditControl>> regionEditControls;
    
    std::shared_ptr<RegionEditComponent> regionEditComponent;
    
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (AudioResourceView)
};
