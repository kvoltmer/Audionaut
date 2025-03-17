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

#include "Interface/Controls/DraggerControl.h"

#include "Engine/Group/AudioTrack.h"
#include "Engine/Group/AudioClip.h"

class SubGroupDraggerControl : public DraggerControl
{
public:
    
    SubGroupDraggerControl(std::shared_ptr<AudiumEngine> audiumEngine,
                           std::shared_ptr<AudioSubGroup> audioSubGroup,
                           std::shared_ptr<ZoomHandler> zoomHandler,
                           juce::Colour colour,
                           std::shared_ptr<RegionSelector> regionSelector) :
        DraggerControl(audiumEngine,
                       zoomHandler,
                       colour,
                       regionSelector),
        audioSubGroup(audioSubGroup)
    {
        regionSelector->subGroupDraggerControls.push_back(this);
    }

    ~SubGroupDraggerControl() override
    {
        std::erase_if(regionSelector->subGroupDraggerControls, [this](const auto* item) {
            return item == this;
        });
    }
        
    bool isSelected() const override
    {
        return audioSubGroup->isSelected();
    }
    
    void setSelected(bool bSelected, bool deselectOthers) override
    {
        if (deselectOthers)
            audioSubGroup->getAudioTrack().getSelectionManager()->deselectAll();
        audioSubGroup->setSelected(bSelected, false);
    }
    
    void shiftSelect() override;
    
    const juce::String getLabelString() const override
    {
        return audioSubGroup->getName();
    }
    
    bool validateData() override;
    
private:
    std::shared_ptr<AudioSubGroup> audioSubGroup;
};
