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
#include "Interface/Views/FadeInOutView.h"

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
                    int rowNumber,
                    std::shared_ptr<PlayListItem> playListItem_) :
        WaveFormViewBase(parentComponent, audiumEngine, audioResource, zoomHandler, audioRegion, colour, regionSelector, rowNumber),
        playListItem(playListItem_)
    {
        // FADE IN OUT VIEW
        fadeInOutView = std::make_unique<FadeInOutView>(playListItem_);
        addAndMakeVisible(fadeInOutView.get());
    }
    
    double getRegionStart(audium::TimeContextType context) const override;
    
    double getClipGain() const override;
    
    void resized() override;
    
private:
    
    std::shared_ptr<PlayListItem> playListItem;
    
    std::unique_ptr<FadeInOutView> fadeInOutView;
    
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (AudioRegionView)
};
