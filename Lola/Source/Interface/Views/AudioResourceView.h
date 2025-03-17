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
#include "Interface/Views/WaveFormViewBase.h"

class AudioResource;
class ZoomHandler;
class AudioRegion;
class RegionSelector;
class RegionEditControl;
class AudiumEngine;

class AudioResourceView  : public WaveFormViewBase
{
public:
    AudioResourceView(juce::Component *parentComponent,
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
    }
    
    double getRegionStart(audium::TimeContextType context) const override;
        
private:
    
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (AudioResourceView)
};
