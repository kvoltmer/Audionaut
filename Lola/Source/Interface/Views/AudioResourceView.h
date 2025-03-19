//    Lola - Audio editing application for multitrack recordings.
//    Copyright (C) 2025 Klaus Voltmer
//
//    Lola uses a GPL/commercial licence - see LICENCE.md for details.

#pragma once

#include <JuceHeader.h>
#include "Interface/Views/WaveFormViewBase.h"
#include "Engine/AudiumEngine.h"
#include "Engine/Resource/AudioResource.h"

class ZoomHandler;
class RegionSelector;
class RegionEditControl;


class AudioResourceView  : public WaveFormViewBase
{
public:
    AudioResourceView(juce::Component *parentComponent,
                      std::shared_ptr<audium::AudiumEngine> audiumEngine,
                      std::shared_ptr<audium::AudioResource> audioResource,
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
    }
    
    double getRegionStart(audium::TimeContextType context) const override;
        
private:
    
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (AudioResourceView)
};
