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
#include "Interface/LookAndFeel/AudiumLookAndFeel.h"
#include "Interface/Controls/RegionSelector.h"

class SliderControl  : public juce::Slider
{
public:
    SliderControl (const juce::String& componentName,
                   std::shared_ptr<RegionSelector> regionSelector_) :
        juce::Slider(componentName),
        regionSelector(regionSelector_)
    {
    }
    
    ~SliderControl() = default;
    
    void mouseEnter (const MouseEvent& e)
    {
        if (regionSelector != nullptr)
            regionSelector->setEnabled(false);
    }

    void mouseExit (const MouseEvent& e)
    {
        if (regionSelector != nullptr)
            regionSelector->setEnabled(true);
    }

private:
    
    std::shared_ptr<RegionSelector> regionSelector;
    
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (SliderControl)
};
