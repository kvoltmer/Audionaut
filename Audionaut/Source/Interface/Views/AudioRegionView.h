//    Audionaut - Audio editing application for multitrack recordings.
//    Copyright (C) 2025 Klaus Voltmer
//
//    Audionaut uses a GPL/commercial licence - see LICENCE.md for details.

#pragma once

#include <JuceHeader.h>
#include <memory>
#include "WaveFormViewBase.h"
#include "Interface/Views/FadeInOutView.h"
#include "Interface/Controls/SliderControl.h"
#include "Interface/Components/MiddlePanel/ChannelView/ChannelComponent.h"
#include "Engine/PlayList/PlayListItem.h"
#include "Engine/AudiumEngine.h"
#include "Engine/Resource/AudioResource.h"

class ZoomHandler;

class AudioRegionView : public WaveFormViewBase
{
public:
    AudioRegionView(juce::Component *parentComponent_,
                    std::shared_ptr<audium::AudiumEngine> audiumEngine_,
                    std::shared_ptr<audium::AudioResource> audioResource_,
                    std::shared_ptr<ZoomHandler> zoomHandler_,
                    juce::Colour colour_,
                    std::shared_ptr<RegionSelector> regionSelector_,
                    int rowNumber_,
                    std::shared_ptr<audium::PlayListItem> playListItem_) :
        WaveFormViewBase(parentComponent_,
                         audiumEngine_,
                         audioResource_,
                         zoomHandler_,
                         colour_,
                         regionSelector_,
                         rowNumber_,
                         playListItem_)
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
    
    void updateUI(std::shared_ptr<audium::AudioResource> audioResource, int theChannel) override;
    
    void setPlayListItem(std::shared_ptr<audium::PlayListItem> item, bool volumeControlVisible);

private:
    
    std::unique_ptr<FadeInOutView> fadeInOutView;
    
    std::unique_ptr<SliderControl> volumeSlider;
    
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (AudioRegionView)
};
