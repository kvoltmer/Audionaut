//    Lola - Audio editing application for multitrack recordings.
//    Copyright (C) 2025 Klaus Voltmer
//
//    This program is free software: you can redistribute it and/or modify
//    it under the terms of the GNU General Public License as published by
//    the Free Software Foundation, either version 3 of the License, or
//    (at your option) any later version.
//
//    This program is distributed in the hope that it will be useful,
//    but WITHOUT ANY WARRANTY; without even the implied warranty of
//    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
//    GNU General Public License for more details.
//
//    You should have received a copy of the GNU General Public License
//    along with this program.  If not, see <https://www.gnu.org/licenses/>.

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
