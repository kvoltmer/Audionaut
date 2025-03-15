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
#include "Interface/Controls/SliderControl.h"
#include "Interface/Components/MiddlePanel/ChannelView/ChannelComponent.h"
#include "Engine/PlayList/PlayListItem.h"

class AudioResource;
class ZoomHandler;
class AudioRegion;

class AudioRegionView : public WaveFormViewBase
{
public:
    AudioRegionView(juce::Component *parentComponent,
                    std::shared_ptr<AudiumEngine> audiumEngine,
                    std::shared_ptr<AudioResource> audioResource,
                    std::shared_ptr<ZoomHandler> zoomHandler,
                    juce::Colour colour,
                    std::shared_ptr<RegionSelector> regionSelector,
                    int rowNumber) :
        WaveFormViewBase(parentComponent,
                         audiumEngine,
                         audioResource,
                         zoomHandler,
                         colour,
                         regionSelector,
                         rowNumber)
    {
        // FADE IN OUT VIEW
        fadeInOutView = std::make_unique<FadeInOutView>();
        addAndMakeVisible(fadeInOutView.get());
        
        // VOLUME
        volumeSlider = std::make_unique<SliderControl>(juce::String(), regionSelector);
        addAndMakeVisible(volumeSlider.get());
        ChannelComponent::configureVolumeSlider(volumeSlider.get(), 36.0);
        
        
    }
    
    double getRegionStart(audium::TimeContextType context) const override;
    
    double getClipGain() const override;
    
    void resized() override;
    
    void updateUI(int theChannel) override;
    
    void setPlayListItem(std::shared_ptr<PlayListItem> item);

private:
    
    std::shared_ptr<PlayListItem> playListItem;
    
    std::unique_ptr<FadeInOutView> fadeInOutView;
    
    std::unique_ptr<SliderControl> volumeSlider;
    
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (AudioRegionView)
};
